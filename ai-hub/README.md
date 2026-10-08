# AI Hub

A keyboard-first launcher for AI assistants, plus a switcher button that
follows you into the assistants themselves. Static files only — no build step,
no framework, no backend, no account.

Built for a Fedora + i3wm laptop where **workspace 4 is the AI workspace**, but
the page itself is just a web page and works anywhere.

```
   $mod+Shift+a                          the ✦ AI button
        │                                  (any chatbot)
        ↓                                       │
   ┌──────────┐                                 │
   │  AI Hub  │  ← workspace 4 ────────────────┘
   └────┬─────┘
        │  ↑ ↓ ← →  then  ↵
        ↓
   ChatGPT · Claude · Gemini · DeepSeek · Grok · Kimi
   Le Chat · Qwen · Perplexity · Copilot · Meta AI · Poe
```

## What is in here

```
ai-hub/
├── index.html            the page
├── style.css
├── app.js                renders the grid, handles the keyboard
├── data/ai.json          THE list of assistants — edit this one
├── bin/
│   ├── ai-hub            launcher: serves the page, opens it as an app window
│   ├── ai-hub-server.py  static files + /open?id=, which launches assistants
│   └── ai-open           opens one URL as an application window
├── userscript/           the ✦ AI button for chatbot sites
│   └── ai-hub.user.js
└── i3/config.example     the i3 bindings and window rules
```

## Using it

```sh
bin/ai-hub            # serve + open in the default browser as an app window
bin/ai-hub --no-browser   # just start the server and print the URL
bin/ai-hub --port 9000
```

| Key | |
|---|---|
| `←` `↓` `↑` `→` | move between assistants |
| `↵` | open the selected one |
| `f` | toggle favourite |
| `/` | jump to search |
| `Esc` | clear search |

Favourites are kept in `localStorage`, so they are per-browser and need no
account. The `"favorite": true` flags in `ai.json` are only the starting set.

## Adding an assistant

Add one object to `data/ai.json`:

```json
{
  "id": "example",
  "name": "Example",
  "provider": "Someone",
  "url": "https://example.com/",
  "domains": ["example.com"],
  "category": ["chat"],
  "color": "#ff0066",
  "icon": "E",
  "blurb": "One line about what it is good at.",
  "favorite": false
}
```

Nothing else needs touching — the grid, the filters and the count all come from
that file. If you want it in the in-page switcher too, add the same entry to the
`AI` array in `userscript/ai-hub.user.js` and a `@match` line for its domain.

## The userscript

Install `userscript/ai-hub.user.js` in Violentmonkey or Tampermonkey. It puts a
small `✦ AI` button in the top-right of every supported chatbot. Click it (or
press `alt`+`a`) for a list of the others; `↑` `↓` to move, `↵` to open in a new
tab, `h` for the hub.

The UI lives in a closed shadow root, so the chatbot's CSS cannot reach it and
it cannot disturb the chatbot's layout. Its key handler runs in the capture
phase because several chatbot sites swallow keystrokes before they bubble.

## Two design decisions worth knowing

**Why a local server instead of opening `index.html` directly.**
Three reasons, and the third is the one that matters.

1. The page loads `data/ai.json` with `fetch()`, and browsers refuse `fetch()`
   from a `file://` page, so a file-URL hub shows an empty grid.
2. It gives the userscript a fixed address to send you back to.
3. **A link cannot open an assistant in its own window.** `target="_blank"` from
   an app window becomes a *tab* in whatever ordinary browser window already
   exists, on whatever workspace that window is sitting on. No window is
   created, so no window-manager rule can place it — and the assistant lands
   wherever general browsing lives instead of on the AI workspace. So a plain
   left click asks the server (`/open?id=…`) instead, and the server runs
   `bin/ai-open`, which starts the assistant as its own `--app` window.

The endpoint takes an **id, not a URL**, and looks it up in `data/ai.json`, so
the only addresses it can ever launch are the ones already listed there. It
listens on loopback only. The server starts only if the port is not already
answering, so running the launcher twice is harmless.

Middle-click, ctrl-click and a statically hosted copy of the page are all
untouched — they follow the ordinary link. Only a plain left click is
intercepted, and if the endpoint is not there the click falls back to opening a
tab.

**Why the assistant list exists twice.**
`data/ai.json` is the source of truth for the page. The userscript keeps its own
copy because a script running on `claude.ai` cannot fetch from `127.0.0.1` —
the browser blocks cross-origin reads, and adding a CORS-enabled service just to
share twelve lines of data would cost more than it saves. Adding an assistant in
two files is the whole maintenance burden of this project.

## Hosting it instead

Any static host works, GitHub Pages included — the page has no server-side
parts. Point `bin/ai-hub` (and `HUB_URL` in the userscript) at the URL and drop
the local server.
