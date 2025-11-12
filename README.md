# 🐼 Brightpanda

### Description

**Brightpanda** is a static code crawler that scans a project directory, detects imports, and maps out internal dependencies between files or modules. It helps developers understand large codebases by producing a structured graph of how components interact.

Brightpanda doesn’t execute code — it statically parses files to extract edges (imports, calls, or references) and outputs them in a machine-readable format such as JSON.

---

### 🧠 Features

* Fast static analysis for projects
* Generates dependency graphs or manifests
* Optional verbose logging for detailed tracing
* Simple command-line interface
* Easy integration with visualization or analysis tools

---

### 🧰 Languages Supported

* **Python**

---

### ⚙️ Command Usage

```bash
brightpanda [path] [options]
```

#### **Arguments**

| Argument | Description                     |
| -------- | ------------------------------- |
| `path`   | Root directory or file to scan. |

---

### 🏴‍☠️ Flags

| Flag                  | Alias | Description                                                              |
| --------------------- | ----- | ------------------------------------------------------------------------ |
| `--no-cache`          | —     | Disable caching and force a full rescan.                                 |
| `--verbose`           | `-v`  | Enable detailed logging output.                                          |
| `--output <filename>` | —     | Write results to a specific file (default: `manifest.json`). |
| `--help`              | `-h`  | Show usage information and exit.                                         |

---

### 💾 Example Commands

```bash
# Basic scan of current directory
brightpanda .

# Scan with verbose logs
brightpanda src/ -v

# Force rescan and save results to a custom file
brightpanda ./project --no-cache --output filename.json
```

---

### 📦 Example Output

```json
{
  "schema_version": "1.0",
  "scan_metadata": {
    "timestamp": "2025-11-10T17:17:39Z",
    "crawler_version": "1.0.0",
    "files_analyzed": 35
  },
  "repo": "example-repo",
  "languages": ["python"],
  "services": [
    {
      "name": "service_alpha",
      "language": "python",
      "path": "/redacted/path/service_alpha/__init__.py",
      "file_count": 5,
      "files": [
        "/redacted/path/service_alpha/main.py",
        "/redacted/path/service_alpha/utils.py"
      ]
    },
    {
      "name": "service_beta",
      "language": "python",
      "path": "/redacted/path/service_beta/app.py",
      "file_count": 3,
      "files": [
        "/redacted/path/service_beta/handler.py"
      ]
    }
  ],
  "endpoints": [
    {"service": "service_alpha", "path": "/list", "method": "GET", "handler": "list_items"},
    {"service": "service_beta", "path": "/upload", "method": "POST", "handler": "upload_file"}
  ],
  "edges": [
    ["service_alpha", "service_beta", "HTTP_CALL", "post"],
    ["tests", "service_alpha", "HTTP_CALL", "get"]
  ]
}

```

---

### 🛠️ Roadmap

* [ ] Add support for other languages like JavaScript/TypeScript, Java, Go
* [ ] Configurable graph output (JSON, CSV, DOT)
* [ ] Visualization mode for dependency graphs
* [ ] Smarter edge classification (import vs. call vs. reference)

---

### 📜 License (MIT)

Brightpanda is distributed under the **MIT License**, which means:

* You’re free to use, modify, and distribute the code (even commercially).
* You must include the original license and copyright notice.
* The author provides the software **“as is”** — no warranty or liability.
