//GEEKS FOR GEEKS IMPLEMENTATION
//Source: https://www.geeksforgeeks.org/trie-insert-and-search/

// C implementation of search and insert operations
// on Trie
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
 
#define ARRAY_SIZE(a) sizeof(a)/sizeof(a[0])
 
// Alphabet size (# of symbols)
#define ALPHABET_SIZE (75)
 
// Converts key current character into index
// use '0' through 'z' and lower case
#define CHAR_TO_INDEX(c) ((int)c - (int)'0')
 
// trie node
struct TrieNode
{
    struct TrieNode *children[ALPHABET_SIZE];
 
    // isEndOfWord is true if the node represents
    // end of a word
    int sd;
    bool isEndOfWord;
};
 
// Returns new trie node (initialized to NULLs)
struct TrieNode *getNode(void)
{
    struct TrieNode *pNode = NULL;
 
    pNode = (struct TrieNode *)malloc(sizeof(struct TrieNode));
 
    if (pNode)
    {
        int i;
 
        pNode->isEndOfWord = false;
        pNode->sd = -1;
        for (i = 0; i < ALPHABET_SIZE; i++) {
            pNode->children[i] = NULL;
        }
    }
 
    return pNode;
}
 
// If not present, inserts key into trie
// If the key is prefix of trie node, just marks leaf node
void insert(struct TrieNode *root, const char *key, int sd)
{
    int level;
    int length = strlen(key);
    int index;
 
    struct TrieNode *pCrawl = root;
 
    for (level = 0; level < length; level++)
    {
        index = CHAR_TO_INDEX(key[level]);
        if (!pCrawl->children[index]) {
            pCrawl->children[index] = getNode();
        }
 
        pCrawl = pCrawl->children[index];
    }
    
    // mark last node as leaf
    pCrawl->isEndOfWord = true;
    pCrawl->sd = sd;
}

//return the socket descriptor of a node, returns 0 if key not in tree.
int return_sd(struct TrieNode *root, const char *key) 
{
    int level;
    int length = strlen(key);
    int index;
    struct TrieNode *pCrawl = root;
    for (level = 0; level < length; level++)
    {
        index = CHAR_TO_INDEX(key[level]);
 
        if (!pCrawl->children[index]) {
            return -1;
        }
 
        pCrawl = pCrawl->children[index];
    }
    return (pCrawl->sd);
}
 
// Returns true if key presents in trie, else false
bool search(struct TrieNode *root, const char *key)
{
    int level;
    int length = strlen(key);
    int index;
    struct TrieNode *pCrawl = root;
 
    for (level = 0; level < length; level++)
    {
        index = CHAR_TO_INDEX(key[level]);
 
        if (!pCrawl->children[index]) {
            return false;
        }
 
        pCrawl = pCrawl->children[index];
    }
 
    return (pCrawl->isEndOfWord);
}

// Returns true if root has no children, else false
bool isEmpty(struct TrieNode* root)
{
    for (int i = 0; i < ALPHABET_SIZE; i++) {
         if (root->children[i]) {
            return false;
        }
    }
       
    return true;
}

struct TrieNode* removeTrieNode(struct TrieNode* root, const char *key, int depth)
{
    // If tree is empty
    if (!root) {
        return NULL;
    }
 
    // If last character of key is being processed
    if (depth == strlen(key)) {
 
        // This node is no more end of word after
        // removal of given key
        if (root->isEndOfWord) {
            root->isEndOfWord = false;
            root->sd = -1;
        }
          
 
        // If given is not prefix of any other word
        if (isEmpty(root)) {
            free(root);
            root = NULL;
        }
 
        return root;
    }
 
    // If not last character, recur for the child
    // obtained using ASCII value

    int index = CHAR_TO_INDEX(key[depth]);

    root->children[index] =
          removeTrieNode(root->children[index], key, depth + 1);
 
    // If root does not have any child (its only child got
    // deleted), and it is not end of another word.
    if (isEmpty(root) && root->isEndOfWord == false && depth) {
        free(root);
        root = NULL;
    }
 
    return root;
}