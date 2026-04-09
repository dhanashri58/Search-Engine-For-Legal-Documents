#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tokenizer.h"
#include "index.h"
#include "search.h"
#include "trie.h"
#include "ranking.h"

static void trim_newline(char *s)
{
    if (!s)
        return;
    s[strcspn(s, "\r\n")] = '\0';
}

static int starts_with(const char *s, const char *prefix)
{
    while (*prefix)
    {
        if (*s++ != *prefix++)
            return 0;
    }
    return 1;
}

static void print_index_statistics(void)
{
    int docs = get_document_count();
    int vocab = get_vocabulary_size();
    long long tokens = get_total_tokens_indexed();
    double avg_len = (docs > 0) ? ((double)tokens / (double)docs) : 0.0;

    printf("\nIndex statistics:\n");
    printf("Documents indexed: %d\n", docs);
    printf("Vocabulary size: %d\n", vocab);
    printf("Total tokens indexed: %lld\n", tokens);
    printf("Average document length: %.2f\n\n", avg_len);
}

static int g_limit = 10; /* Top-K results to display */

static void print_help(void) {
    printf("\n"
           "+-------------------------------------------------------+\n"
           "|        LEGAL SEARCH ENGINE - COMMAND CENTER           |\n"
           "+-------------------------------------------------------+\n"
           "| SEARCH COMMANDS:                                      |\n"
           "|  - Just type keywords: Search (AND) e.g. 'contract'    |\n"
           "|  - keyword OR keyword: Search (OR)                    |\n"
           "|  - ac <prefix>       : Autocomplete (Trie)            |\n"
           "|                                                       |\n"
           "| ADS CONTROLS:                                         |\n"
           "|  - meta <label>      : Metadata Search (AVL)          |\n"
           "|  - tree              : View Metadata Tree (AVL)       |\n"
           "|  - rank [tf|tfidf]   : Change Ranking Metric          |\n"
           "|  - limit <N>         : Set Top-K display limit (Heap) |\n"
           "|                                                       |\n"
           "| UTILS:                                                |\n"
           "|  - stats             : Index Statistics               |\n"
           "|  - debug             : View Entire Index (Large)      |\n"
           "|  - list <word>       : View Posting List of a word    |\n"
           "|  - help              : Show this menu                 |\n"
           "|  - quit              : Exit Application               |\n"
           "+-------------------------------------------------------+\n");
}

int main(void)
{
    printf("\n[Initializing Engine...]\n");
    index_init();
    trie_init();

    tokenize_directory("data");
    int n_docs = get_document_count();

    printf("+-------------------------------------------------------+\n");
    printf("| SEARCH ENGINE LOADED: %d documents indexed             |\n", n_docs);
    printf("+-------------------------------------------------------+\n");
    print_help();

    char line[1024];
    while (1)
    {
        printf("\nADS@Search >> ");
        if (!fgets(line, (int)sizeof(line), stdin))
            break;
        trim_newline(line);
        if (line[0] == '\0')
            continue;

        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0)
            break;

        if (strcmp(line, "help") == 0) {
            print_help();
            continue;
        }

        if (strcmp(line, "stats") == 0)
        {
            print_index_statistics();
            continue;
        }

        if (strcmp(line, "debug") == 0)
        {
            debug_print_index();
            continue;
        }

        if (starts_with(line, "list "))
        {
            char *word = line + 5;
            while (*word == ' ') word++;
            if (*word == '\0') {
                printf("(usage) list <word>\n");
                continue;
            }
            debug_word(word);
            continue;
        }

        if (starts_with(line, "limit "))
        {
            int n = atoi(line + 6);
            if (n > 0) {
                g_limit = n;
                printf("Result display limit set to: Top-%d\n", g_limit);
            } else {
                printf("(error) Please provide a valid number > 0\n");
            }
            continue;
        }

        if (starts_with(line, "ac "))
        {
            char *prefix = line + 3;
            while (*prefix == ' ') prefix++;
            if (*prefix == '\0') {
                printf("(usage) ac <prefix>\n");
                continue;
            }
            printf("--- Trie Autocomplete Suggestions ---\n");
            autocomplete(prefix);
            continue;
        }

        if (starts_with(line, "meta "))
        {
            char *label = line + 5;
            while (*label == ' ') label++;
            if (*label == '\0') {
                printf("(usage) meta <filename>\n");
                continue;
            }
            int id = get_docID_by_label(label);
            if (id != -1) {
                printf("AVL Result: '%s' found (ID: %d)\n", label, id);
            } else {
                printf("AVL Result: Document not found.\n");
            }
            continue;
        }

        if (strcmp(line, "tree") == 0)
        {
            print_metadata_tree();
            continue;
        }

        if (starts_with(line, "rank "))
        {
            char *mode = line + 5;
            while (*mode == ' ') mode++;
            if (strcmp(mode, "tf") == 0) {
                ranking_set_mode(RANK_TF);
                printf("Ranking Algorithm: TF (Term Frequency)\n");
            } else if (strcmp(mode, "tfidf") == 0) {
                ranking_set_mode(RANK_TFIDF);
                printf("Ranking Algorithm: TF-IDF (Advanced Retrieval)\n");
            } else {
                printf("(usage) rank tf | rank tfidf\n");
            }
            continue;
        }

        /* DEFAULT: Search */
        SearchResult *results = NULL;
        int count = 0;
        if (!search_query(line, &results, &count)) {
            printf("Error in search.\n");
            continue;
        }

        if (count == 0) {
            printf("No results found for '%s'\n", line);
            free(results);
            continue;
        }

        printf("\n--- Displaying Top %d of %d results ---\n", (count < g_limit ? count : g_limit), count);
        for (int i = 0; i < count && i < g_limit; i++)
        {
            const char *label = get_document_label(results[i].docID);
            printf("[%d] %s (Score: %.2f)\n", i + 1, label ? label : "Unknown", results[i].score);
        }

        free(results);
    }

    index_free();
    trie_free();
    return 0;
}
