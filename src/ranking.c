#include <stdio.h>
#include <string.h>

#include "ranking.h"

static RankingMode g_mode = RANK_TF;

void ranking_set_mode(RankingMode mode) {
    g_mode = mode;
}

RankingMode ranking_get_mode(void) {
    return g_mode;
}

static void swap(SearchResult* a, SearchResult* b) {
    SearchResult t = *a;
    *a = *b;
    *b = t;
}

/* 
 * MAX-HEAP LOGIC (Unit 2: Heaps and Priority Queues)
 * Logic: Ensure parent is always greater than children.
 */
static void max_heapify(SearchResult* arr, int n, int i) {
    int largest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < n && (arr[left].score > arr[largest].score || 
       (arr[left].score == arr[largest].score && arr[left].docID < arr[largest].docID)))
        largest = left;

    if (right < n && (arr[right].score > arr[largest].score || 
       (arr[right].score == arr[largest].score && arr[right].docID < arr[largest].docID)))
        largest = right;

    if (largest != i) {
        swap(&arr[i], &arr[largest]);
        max_heapify(arr, n, largest);
    }
}

void rank_results(SearchResult* results, int count) {
    rank_results_mode(results, count, g_mode);
}

void rank_results_mode(SearchResult* results, int count, RankingMode mode) {
    if (!results || count <= 1) return;
    (void)mode;

    /* Step 1: Build Max-Heap (Rearrange array) */
    for (int i = count / 2 - 1; i >= 0; i--) {
        max_heapify(results, count, i);
    }

    /* Step 2: Extract top elements to the end of array (Heap Sort logic) */
    for (int i = count - 1; i >= 0; i--) {
        swap(&results[0], &results[i]);
        max_heapify(results, i, 0);
    }
    
    for (int i = 0; i < count / 2; i++) {
        swap(&results[i], &results[count - 1 - i]);
    }
}

