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

int main(void)
{
    index_init();
    trie_init();

    /* Index everything under data/ (recursive .txt crawl on Windows). */
    tokenize_directory("data");
    int n_docs = get_document_count();
    if (n_docs <= 0)
    {
        printf("No documents indexed. Put .txt files under `data/` (including subfolders).\n");
    }
    else
    {
        printf("Search Engine Ready (%d documents indexed)\n", n_docs);
    }

    printf("Commands:\n");
    printf("  - Enter keywords to search (default AND). Example: contract breach\n");
    printf("  - Use OR for union. Example: contract OR criminal\n");
    printf("  - Autocomplete: ac <prefix>   Example: ac con\n");
    printf("  - Ranking mode: rank tf | rank tfidf\n");
    printf("  - Index statistics: stats\n");
    printf("  - Debug index: debug\n");
    printf("  - Quit: quit\n\n");

    char line[1024];
    while (1)
    {
        printf("Enter query: ");
        if (!fgets(line, (int)sizeof(line), stdin))
            break;
        trim_newline(line);
        if (line[0] == '\0')
            continue;

        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0)
            break;

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

        if (starts_with(line, "ac "))
        {
            char *prefix = line + 3;
            while (*prefix == ' ')
                prefix++;
            if (*prefix == '\0')
            {
                printf("(usage) ac <prefix>\n");
                continue;
            }
            printf("Suggestions:\n");
            autocomplete(prefix);
            continue;
        }

        if (starts_with(line, "rank "))
        {
            char *mode = line + 5;
            while (*mode == ' ')
                mode++;
            if (strcmp(mode, "tf") == 0)
            {
                ranking_set_mode(RANK_TF);
                printf("Ranking mode set to TF\n");
            }
            else if (strcmp(mode, "tfidf") == 0)
            {
                ranking_set_mode(RANK_TFIDF);
                printf("Ranking mode set to TF-IDF\n");
            }
            else
            {
                printf("(usage) rank tf | rank tfidf\n");
            }
            continue;
        }

        SearchResult *results = NULL;
        int count = 0;
        if (!search_query(line, &results, &count))
        {
            printf("Search failed (out of memory)\n");
            continue;
        }

        if (count == 0)
        {
            printf("No results\n");
            free(results);
            continue;
        }

        printf("\nResults:\n");
        int top = count;
        if (top > 10)
            top = 10;
        for (int i = 0; i < top; i++)
        {
            const char *label = get_document_label(results[i].docID);
            if (!label)
                label = "(unknown)";
            printf("%s (score %.2f)\n", label, results[i].score);
        }
        printf("\n");

        free(results);
    }

    index_free();
    trie_free();
    return 0;
}
