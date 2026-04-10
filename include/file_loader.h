#ifndef FILE_LOADER_H
#define FILE_LOADER_H

/*
Directory/document loading abstraction.

Today:
- Windows-only recursive crawl using `dir /b /s /a-d "<root>"` via popen.
- Built-in readers for text-like files such as `.txt`, `.md`, `.csv`, `.json`.
- Best-effort extractors for `.pdf`, `.doc`, `.docx`, `.rtf`, `.pptx`, `.xlsx`.

Future:
- Replace implementation with native directory APIs per platform.
*/

typedef void (*DocumentSinkFn)(const char* label, const char* text, void* user);

/* Register a callback invoked once per discovered document. */
void file_loader_set_sink(DocumentSinkFn sink, void* user);

/*
Load all documents under `path` and push them into the configured sink.
Returns number of documents loaded (0 on none/error).
*/
int load_documents_from_directory(const char* path);

#endif

