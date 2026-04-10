# High-Speed Legal Text Search Engine (C99)

A **console-based search engine** that indexes legal documents from the `data/` folder (including subfolders like **judgments**, **contracts**, **acts**, **case_laws**) and supports:

- **Inverted index** (word → posting list of documents + term frequencies)
- **AVL Tree** (Metadata search: find documents by filename in O(log N))
- **Max-Heap** (Efficient ranking: extracts Top-K results dynamically)
- **Fast multi-keyword search** (default **AND** via posting list intersection)
- **Ranking** by TF-IDF or TF score: \(score = \sum term\_frequency\)
- **Autocomplete** using a **Trie**

Only standard C headers are used: `stdio.h`, `stdlib.h`, `string.h`, `ctype.h`.

---

## Project structure

```
data/
  judgments/
  contracts/
  acts/
  case_laws/

include/
  structures.h
  tokenizer.h
  file_loader.h
  index.h
  query_parser.h
  search.h
  ranking.h
  trie.h

src/
  src\tokenizer.c
  src\file_loader.c
  src\index.c
  src\query_parser.c
  src\search.c
  src\ranking.c
  src\trie.c
  src\avl_tree.c
  src\main.c

IMPLEMENTED.md (Detailed ADS Guide)
Makefile

```

---

## How indexing works

- On **Windows**, the program **recursively crawls** the `data/` directory and indexes supported files it finds.
- Each file becomes one “document”.
- Words are normalized:
  - lowercased
  - punctuation removed
  - tokenized on non-alphanumeric characters
- Supported inputs:
  - Direct text read: `.txt`, `.md`, `.csv`, `.json`, `.xml`, `.html`, `.htm`, `.log`
  - PowerShell/.NET extraction: `.docx`, `.pptx`, `.xlsx`
  - Best-effort Windows Word automation fallback: `.pdf`, `.doc`, `.rtf`

The result list prints the **file path** as the document label, so you can see which folder it came from.

---

## Build

### Option A: Makefile (recommended)

If you have `make` installed:

```bash
make
```

This produces `search_engine` (or `search_engine.exe` on Windows).

### Option B: Direct GCC (Windows / MinGW)

```powershell
gcc -Iinclude -Wall -Wextra -std=c99 -O2 -o search_engine.exe src\main.c src\tokenizer.c src\file_loader.c src\index.c src\search.c src\query_parser.c src\ranking.c src\trie.c src\avl_tree.c



```

---

## Run

### Windows

```powershell
.\search_engine.exe
```

### Web UI

Run the Flask interface from the project root:

```powershell
python ui\app.py
```

Then open `http://127.0.0.1:5000/`.

If you open `ui\templates\index.html` directly in the browser, the Jinja template tags such as `{{ ... }}` and `{% ... %}` will appear as raw text because the file is not being rendered by Flask.

### Linux/macOS (if you later port directory crawl)

```bash
./search_engine
```

---

## Usage

Once running:

- **AND search (default)**:
  - `contract breach`
- **OR search**:
  - `contract OR criminal`
- **Autocomplete (Trie)**:
  - `ac con`
- **Metadata (AVL Trees)**:
  - `meta doc1.txt`
  - `tree` (View visual tree)
- **Index Inspection**:
  - `list <word>` (Show posting list for a word)
  - `limit <N>` (Set Top-K result limit)
- **Ranking Control**:
  - `rank tf` | `rank tfidf`
- **Quit**:
  - `quit`

Example output:

```
Results:
data\contracts\nda.txt (score 3)
data\judgments\case_101.txt (score 2)
```

---

## Notes / limitations

- `.pdf`, `.doc`, and `.rtf` extraction is **best-effort**. On Windows, the loader first tries built-in tools and then falls back to Microsoft Word automation if Word is installed.
- `.docx`, `.pptx`, and `.xlsx` extraction uses PowerShell + .NET ZIP/XML parsing, so heavily formatted files may lose layout but the searchable text is preserved.
- `data\documents.txt` is treated as a legacy sample/manifest and is **skipped** during directory indexing.

