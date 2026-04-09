#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avl_tree.h"

static int max(int a, int b) {
    return (a > b) ? a : b;
}

static int height(AVLNode* n) {
    return n ? n->height : 0;
}

static AVLNode* create_node(const char* key, int docID) {
    AVLNode* node = (AVLNode*)malloc(sizeof(AVLNode));
    if (!node) return NULL;
    /* Manual string duplication for C99 compatibility */
    node->key = (char*)malloc(strlen(key) + 1);
    if (node->key) strcpy(node->key, key);
    node->docID = docID;
    node->left = node->right = NULL;
    node->height = 1;
    return node;
}

/* Right Rotate */
static AVLNode* right_rotate(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = max(height(y->left), height(y->right)) + 1;
    x->height = max(height(x->left), height(x->right)) + 1;

    return x;
}

/* Left Rotate */
static AVLNode* left_rotate(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = max(height(x->left), height(x->right)) + 1;
    y->height = max(height(y->left), height(y->right)) + 1;

    return y;
}

static int get_balance(AVLNode* n) {
    return n ? height(n->left) - height(n->right) : 0;
}

AVLNode* avl_insert(AVLNode* node, const char* key, int docID) {
    /* 1. Normal BST insertion */
    if (node == NULL) return create_node(key, docID);

    if (strcmp(key, node->key) < 0)
        node->left = avl_insert(node->left, key, docID);
    else if (strcmp(key, node->key) > 0)
        node->right = avl_insert(node->right, key, docID);
    else
        return node; /* Duplicate keys not allowed */

    /* 2. Update height */
    node->height = 1 + max(height(node->left), height(node->right));

    /* 3. Check balance and balance the tree */
    int balance = get_balance(node);

    // Left Left Case
    if (balance > 1 && strcmp(key, node->left->key) < 0)
        return right_rotate(node);

    // Right Right Case
    if (balance < -1 && strcmp(key, node->right->key) > 0)
        return left_rotate(node);

    // Left Right Case
    if (balance > 1 && strcmp(key, node->left->key) > 0) {
        node->left = left_rotate(node->left);
        return right_rotate(node);
    }

    // Right Left Case
    if (balance < -1 && strcmp(key, node->right->key) < 0) {
        node->right = right_rotate(node->right);
        return left_rotate(node);
    }

    return node;
}

AVLNode* avl_search(AVLNode* root, const char* key) {
    if (root == NULL) return NULL;

    int cmp = strcmp(key, root->key);
    if (cmp == 0) return root;

    if (cmp < 0)
        return avl_search(root->left, key);

    return avl_search(root->right, key);
}

static void print_avl_internal(AVLNode* root, char* prefix, int isLeft) {
    if (root == NULL) return;

    printf("%s", prefix);
    printf(isLeft ? "|-- " : "L-- ");
    
    const char* last_s = strrchr(root->key, '\\');
    if (!last_s) last_s = strrchr(root->key, '/');
    const char* display = last_s ? last_s + 1 : root->key;
    printf("[%s | ID:%d]\n", display, root->docID);

    char new_prefix[256];
    snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, isLeft ? "|   " : "    ");

    print_avl_internal(root->left, new_prefix, 1);
    print_avl_internal(root->right, new_prefix, 0);
}

void avl_print_visual(AVLNode* root, int space) {
    (void)space;
    if (!root) return;
    print_avl_internal(root, "", 0);
}

void avl_free(AVLNode* root) {
    if (root) {
        avl_free(root->left);
        avl_free(root->right);
        free(root->key);
        free(root);
    }
}
