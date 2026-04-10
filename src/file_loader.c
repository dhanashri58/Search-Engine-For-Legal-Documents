#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "file_loader.h"

#ifdef _WIN32
/* MinGW may not declare popen/pclose in strict C99 mode. */
FILE* popen(const char* command, const char* mode);
int pclose(FILE* stream);
#endif

static DocumentSinkFn g_sink = NULL;
static void* g_sink_user = NULL;

void file_loader_set_sink(DocumentSinkFn sink, void* user) {
    g_sink = sink;
    g_sink_user = user;
}

static void rtrim_newline(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static int equals_ignore_case(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (tolower(ca) != tolower(cb)) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static const char* get_leaf_name(const char* path) {
    const char* leaf = strrchr(path, '\\');
    if (!leaf) leaf = strrchr(path, '/');
    return leaf ? (leaf + 1) : path;
}

static const char* get_extension(const char* path) {
    const char* leaf = get_leaf_name(path);
    const char* dot = strrchr(leaf, '.');
    return dot ? dot : "";
}

static int is_plain_text_extension(const char* ext) {
    static const char* k_plain[] = {
        ".txt", ".md", ".csv", ".json", ".xml", ".html", ".htm", ".log"
    };
    int n = (int)(sizeof(k_plain) / sizeof(k_plain[0]));
    for (int i = 0; i < n; i++) {
        if (equals_ignore_case(ext, k_plain[i])) return 1;
    }
    return 0;
}

static int is_openxml_extension(const char* ext) {
    return equals_ignore_case(ext, ".docx")
        || equals_ignore_case(ext, ".pptx")
        || equals_ignore_case(ext, ".xlsx");
}

static int is_word_automation_extension(const char* ext) {
    return equals_ignore_case(ext, ".pdf")
        || equals_ignore_case(ext, ".doc")
        || equals_ignore_case(ext, ".docx")
        || equals_ignore_case(ext, ".rtf");
}

static int is_supported_extension(const char* ext) {
    return is_plain_text_extension(ext)
        || is_openxml_extension(ext)
        || is_word_automation_extension(ext);
}

static char* read_entire_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static char* read_pipe_output(FILE* pipe) {
    if (!pipe) return NULL;

    size_t cap = 4096;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    char chunk[1024];
    while (fgets(chunk, (int)sizeof(chunk), pipe)) {
        size_t chunk_len = strlen(chunk);
        if (len + chunk_len + 1 > cap) {
            size_t new_cap = cap;
            while (len + chunk_len + 1 > new_cap) {
                if (new_cap > ((size_t)-1) / 2) {
                    free(buf);
                    return NULL;
                }
                new_cap *= 2;
            }
            char* grown = (char*)realloc(buf, new_cap);
            if (!grown) {
                free(buf);
                return NULL;
            }
            buf = grown;
            cap = new_cap;
        }
        memcpy(buf + len, chunk, chunk_len);
        len += chunk_len;
    }

    buf[len] = '\0';
    return buf;
}

static char* ps_single_quote_escape(const char* s) {
    size_t extra = 0;
    for (const char* p = s; *p; p++) {
        if (*p == '\'') extra++;
    }

    size_t n = strlen(s);
    char* out = (char*)malloc(n + extra + 1);
    if (!out) return NULL;

    size_t w = 0;
    for (const char* p = s; *p; p++) {
        out[w++] = *p;
        if (*p == '\'') out[w++] = '\'';
    }
    out[w] = '\0';
    return out;
}

static char* extract_with_command(const char* command) {
    FILE* pipe = popen(command, "r");
    if (!pipe) return NULL;

    char* output = read_pipe_output(pipe);
    pclose(pipe);
    return output;
}

static char* extract_text_from_pdf_via_pdftotext(const char* path) {
    char* escaped = ps_single_quote_escape(path);
    if (!escaped) return NULL;

    char cmd[4096];
    snprintf(
        cmd,
        sizeof(cmd),
        "powershell -NoProfile -Command \"$path='%s'; "
        "$cmd=Get-Command pdftotext -ErrorAction SilentlyContinue; "
        "if ($cmd) { & $cmd.Source -layout -nopgbrk -q $path - }\"",
        escaped
    );
    free(escaped);

    return extract_with_command(cmd);
}

static char* extract_text_from_openxml_via_powershell(const char* path, const char* ext) {
    char* escaped = ps_single_quote_escape(path);
    if (!escaped) return NULL;

    char cmd[8192];
    snprintf(
        cmd,
        sizeof(cmd),
        "powershell -NoProfile -Command \"$ErrorActionPreference='Stop'; "
        "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
        "$path='%s'; "
        "$archive=[IO.Compression.ZipFile]::OpenRead($path); "
        "try { "
        "$entries=@(); "
        "if ('%s' -ieq '.docx') { "
        "$entries=@('word/document.xml','word/header1.xml','word/header2.xml','word/header3.xml','word/footer1.xml','word/footer2.xml','word/footer3.xml','word/footnotes.xml','word/endnotes.xml'); "
        "} elseif ('%s' -ieq '.pptx') { "
        "$entries=@($archive.Entries | Where-Object { $_.FullName -like 'ppt/slides/slide*.xml' } | Sort-Object FullName | ForEach-Object { $_.FullName }); "
        "} elseif ('%s' -ieq '.xlsx') { "
        "$entries=@('xl/sharedStrings.xml') + @($archive.Entries | Where-Object { $_.FullName -like 'xl/worksheets/*.xml' } | Sort-Object FullName | ForEach-Object { $_.FullName }); "
        "} "
        "$parts=New-Object System.Collections.Generic.List[string]; "
        "foreach ($name in $entries) { "
        "$entry=$archive.GetEntry($name); "
        "if ($null -eq $entry) { continue } "
        "$reader=New-Object IO.StreamReader($entry.Open()); "
        "try { $xml=$reader.ReadToEnd() } finally { $reader.Dispose() } "
        "$text=[Text.RegularExpressions.Regex]::Replace($xml, '<[^>]+>', ' '); "
        "if (-not [string]::IsNullOrWhiteSpace($text)) { $parts.Add($text) } "
        "} "
        "$joined=[string]::Join(' ', $parts); "
        "$decoded=[Net.WebUtility]::HtmlDecode($joined); "
        "$clean=[Text.RegularExpressions.Regex]::Replace($decoded, '\\s+', ' ').Trim(); "
        "Write-Output $clean; "
        "} finally { $archive.Dispose() }\"",
        escaped,
        ext,
        ext,
        ext
    );
    free(escaped);

    return extract_with_command(cmd);
}

static char* extract_text_via_word_automation(const char* path) {
    char* escaped = ps_single_quote_escape(path);
    if (!escaped) return NULL;

    char cmd[4096];
    snprintf(
        cmd,
        sizeof(cmd),
        "powershell -NoProfile -Command \"$ErrorActionPreference='Stop'; "
        "$path='%s'; "
        "$word=$null; $doc=$null; "
        "try { "
        "$word=New-Object -ComObject Word.Application; "
        "$word.Visible=$false; "
        "$word.DisplayAlerts=0; "
        "$doc=$word.Documents.Open($path, $false, $true); "
        "$text=$doc.Content.Text; "
        "Write-Output ([Text.RegularExpressions.Regex]::Replace($text, '\\s+', ' ').Trim()); "
        "} catch { } "
        "finally { "
        "if ($doc -ne $null) { $doc.Close($false) | Out-Null } "
        "if ($word -ne $null) { $word.Quit() | Out-Null } "
        "}\"",
        escaped
    );
    free(escaped);

    return extract_with_command(cmd);
}

static char* load_document_text(const char* path) {
    const char* ext = get_extension(path);

    if (is_plain_text_extension(ext)) {
        return read_entire_file(path);
    }

    if (equals_ignore_case(ext, ".pdf")) {
        char* pdf_text = extract_text_from_pdf_via_pdftotext(path);
        if (pdf_text && pdf_text[0] != '\0') return pdf_text;
        free(pdf_text);
        return extract_text_via_word_automation(path);
    }

    if (is_openxml_extension(ext)) {
        char* openxml_text = extract_text_from_openxml_via_powershell(path, ext);
        if (openxml_text && openxml_text[0] != '\0') return openxml_text;
        free(openxml_text);
    }

    if (is_word_automation_extension(ext)) {
        return extract_text_via_word_automation(path);
    }

    return NULL;
}

int load_documents_from_directory(const char* path) {
    if (!path || !path[0]) return 0;
    if (!g_sink) return 0;

#ifdef _WIN32
    /*
    Windows recursive crawl:
    - list every file under `path`
    - choose supported extensions in C
    - extract text per file type
    */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "dir /b /s /a-d \"%s\"", path);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        printf("Error: unable to list files under %s\n", path);
        return 0;
    }

    int loaded = 0;
    int skipped = 0;
    int failed = 0;
    char file_path[1024];
    while (fgets(file_path, (int)sizeof(file_path), pipe)) {
        rtrim_newline(file_path);
        if (file_path[0] == '\0') continue;

        const char* leaf = get_leaf_name(file_path);
        if (equals_ignore_case(leaf, "documents.txt")) continue;

        const char* ext = get_extension(file_path);
        if (!is_supported_extension(ext)) {
            skipped++;
            continue;
        }

        char* content = load_document_text(file_path);
        if (!content || content[0] == '\0') {
            failed++;
            free(content);
            continue;
        }

        g_sink(file_path, content, g_sink_user);
        loaded++;
        free(content);
    }

    pclose(pipe);

    if (skipped > 0 || failed > 0) {
        printf(
            "[Loader] Indexed %d files, skipped %d unsupported files, failed to extract %d supported files.\n",
            loaded,
            skipped,
            failed
        );
    }

    return loaded;
#else
    (void)path;
    printf("Directory indexing is only implemented on Windows builds.\n");
    return 0;
#endif
}
