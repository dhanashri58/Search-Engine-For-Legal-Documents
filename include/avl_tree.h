#ifndef AVL_TREE_H
#define AVL_TREE_H

/* 
 * Unit 1: Advanced Trees - AVL Implementation
 * Used for storing and searching document metadata (labels) efficiently.
 */

typedef struct AVLNode {
    char* key;          /* Document label/filename */
    int docID;          /* Associated ID in the index */
    int height;
    struct AVLNode *left;
    struct AVLNode *right;
} AVLNode;

/* Core AVL functions */
AVLNode* avl_insert(AVLNode* node, const char* key, int docID);
AVLNode* avl_search(AVLNode* root, const char* key);
void avl_print_visual(AVLNode* root, int space);
void avl_free(AVLNode* root);

#endif
