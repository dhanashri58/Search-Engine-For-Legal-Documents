# Project Implementation Guide: Search Engine for Legal Documents

Ye guide tere project ki "Advanced Data Structures (ADS)" implementation ko ekdm basic se samjhati hai. Isse tu Viva mein sir ko pura backend flow dikha sakta hai.

---

## 0. Quick Start (How to Run)

Use these commands in your terminal to compile and run the engine:

1. **Compile:**
   ```powershell
   gcc -Iinclude -Wall -Wextra -std=c99 -O2 -o search_engine.exe src\main.c src\tokenizer.c src\file_loader.c src\index.c src\search.c src\query_parser.c src\ranking.c src\trie.c src\avl_tree.c
   ```
2. **Run:**
   ```powershell
   .\search_engine.exe
   ```

---

## 1. Core Data Structures (The ADS Foundation)

Humne syllabus ke alag-alag units ko backend mein integrate kiya hai:

| Component | ADS Used | Syllabus Unit | Purpose |
| :--- | :--- | :--- | :--- |
| **Metadata Manager** | **AVL Tree** | Unit 1 | Filenames aur unki DocID ko balanced tree mein store karna taaki search O(log N) ho. Command: `meta <name>` aur `tree` |
| **Result Ranker** | **Max-Heap** | Unit 2 | Top results dikhane ke liye bina pura array sort kiye results nikalna. Command: `limit <N>` |
| **Autocomplete** | **Trie** | Unit 3 | Type karte waqt suggestions dena. Command: `ac <prefix>` |
| **Inverted Index** | **Hash Table (Chaining)** | Unit 6 | Words se unke documents ki list (Posting List) tak turant pahunchna. Command: `list <word>` |

---

## 2. Process Flow: Step-by-Step

### Step 1: Document Loading (Metadata Indexing)
Jab system ek file (`case1.txt`) uthata hai, toh wo sabse pehle uska naam **AVL Tree** mein daalta hai.
*   **Kyun?** Taaki agar humein sirf metadata search karna ho (e.g., "Check if case1 exists"), toh hum pure thousands of docs scan na karein, balki Tree traversal (Left/Right) karke check karein.

### Step 2: Tokenization (Clean-up)
Backend file ke content ko words mein todta hai. 
*   **Normalization:** Saare words ko lowercase karta hai aur punctuation (!, @, ?) hata deta hai.
*   **Stopword Removal:** "the", "and", "is" jaise common words ko filter out kar deta hai kyunki inka search mein koi value nahi hai.

### Step 3: Inverted Index (The Brain)
Ye sabse important part hai. Yahan do main concept hain:
1.  **Buckets (Hash Table):** Hum word ka hash nikalte hain (`word -> hash ID`) aur usse ek "Bucket" mein store karte hain.
2.  **Posting List (Linked List):** Har word ke saath ek list judi hoti hai jo batati hai ki wo word kaun-kaun se `docID` mein present hai aur kitni baar (`frequency`).

---

## 3. Example Walk-through

Maan lo hamare paas ye 2 documents hain:
*   **Doc 1:** "Contract breach in legal case."
*   **Doc 2:** "Legal breach found."

### Tracking word: "Breach"
1.  **Tokenization:** "Breach" normalize hoke "breach" ban jayega.
2.  **Hashing:** "breach" ka hash niklega (e.g., Bucket #105).
3.  **Posting List Creation:** Bucket 105 mein hum entry karenge:
    *   **breach** -> `[Doc 1, Freq: 1]` -> `[Doc 2, Freq: 1]` -> NULL

Jab tu "breach" search karega, toh system seedha Bucket #105 par jayega aur ye list utha kar tuze results dikha dega.

---

## 4. Ranking and Search

Jab tu query likhta hai (e.g., "legal breach"), toh system:
1.  "legal" ki posting list aur "breach" ki posting list ka **Intersection** (AND logic) nikalta hai.
2.  Phir har document ko ek **Score** deta hai (TF-IDF logic).
3.  **Heap Logic:** In saare scores ko ek **Max-Heap** mein daalta hai. Heap ka "Root" hamesha sabse high-score wala result hota hai. Hum Top-10 results nikal kar display kar dete hain.

---

## 5. Main Structs Explained (C Code Parts)

*   **`struct TrieNode`**: 26 letters ke pointers (children[26]) rakhta hai. `isEndOfWord` flag batata hai ki yahan word khatam hua.
*   **`struct AVLNode`**: `key` (filename), `docID`, aur `height` rakhta hai balancing ke liye.
*   **`struct Posting`**: Ek linked list ka node hai jo `docID`, `frequency`, aur `next` ka address store karta hai.
*   **`struct WordEntry`**: Hash table ki entry jo word aur uski `PostingList` ka head pointer store karti hai.

---

### Key Viva Points to Remember:
1.  **AVL Tree** hamesha balanced rehta hai (height difference max 1), isliye indexing slow nahi hoti.
2.  **Heap** use karne se humein 1 million results sort nahi karne padte, hum sirf top elements extract karke efficiency badhate hain.
3.  **Trie** prefix searching ke liye best hai (O(L) complexity, jahan L word ki length hai).
