// ==UserScript==
// @name         AI Hub switcher
// @namespace    https://github.com/ai-hub
// @version      1.0.0
// @description  A ✦ AI button on every major chatbot: jump straight to another one, or back to your AI Hub.
// @author       you
// @match        https://chatgpt.com/*
// @match        https://chat.openai.com/*
// @match        https://claude.ai/*
// @match        https://gemini.google.com/*
// @match        https://grok.com/*
// @match        https://chat.deepseek.com/*
// @match        https://www.kimi.com/*
// @match        https://kimi.moonshot.cn/*
// @match        https://chat.mistral.ai/*
// @match        https://chat.qwen.ai/*
// @match        https://www.perplexity.ai/*
// @match        https://copilot.microsoft.com/*
// @match        https://www.meta.ai/*
// @match        https://poe.com/*
// @match        http://127.0.0.1:8765/*
// @run-at       document-idle
// @grant        none
// ==/UserScript==

/* This list is a deliberate copy of data/ai.json.
 *
 * The hub page loads that file over http from 127.0.0.1; a userscript running
 * on claude.ai cannot, because the browser will not let a page on one origin
 * fetch another. Rather than add a CORS-enabled service just to share twelve
 * lines of data, the list lives in both places. If you add an assistant, add it
 * to data/ai.json AND here — that is the only maintenance cost of the whole
 * project, and it is a deliberate trade for having no backend.
 */
(function () {
  "use strict";

  const HUB_URL = "http://127.0.0.1:8765/";

  const AI = [
    { id: "chatgpt",    name: "ChatGPT",    url: "https://chatgpt.com/",              domains: ["chatgpt.com", "chat.openai.com"], color: "#10a37f", icon: "✳" },
    { id: "claude",     name: "Claude",     url: "https://claude.ai/new",             domains: ["claude.ai"],                      color: "#d97757", icon: "C" },
    { id: "gemini",     name: "Gemini",     url: "https://gemini.google.com/",        domains: ["gemini.google.com"],              color: "#4285f4", icon: "✦" },
    { id: "deepseek",   name: "DeepSeek",   url: "https://chat.deepseek.com/",        domains: ["chat.deepseek.com"],              color: "#4d78ff", icon: "D" },
    { id: "grok",       name: "Grok",       url: "https://grok.com/",                 domains: ["grok.com"],                       color: "#8a8f9c", icon: "𝕏" },
    { id: "kimi",       name: "Kimi",       url: "https://www.kimi.com/",             domains: ["kimi.com", "kimi.moonshot.cn"],   color: "#695cff", icon: "K" },
    { id: "mistral",    name: "Le Chat",    url: "https://chat.mistral.ai/",          domains: ["chat.mistral.ai"],                color: "#ff7000", icon: "M" },
    { id: "qwen",       name: "Qwen",       url: "https://chat.qwen.ai/",             domains: ["chat.qwen.ai"],                   color: "#6257ee", icon: "Q" },
    { id: "perplexity", name: "Perplexity", url: "https://www.perplexity.ai/",        domains: ["perplexity.ai"],                  color: "#20b8cd", icon: "P" },
    { id: "copilot",    name: "Copilot",    url: "https://copilot.microsoft.com/",    domains: ["copilot.microsoft.com"],          color: "#36a5ff", icon: "◎" },
    { id: "metaai",     name: "Meta AI",    url: "https://www.meta.ai/",              domains: ["meta.ai"],                        color: "#087cff", icon: "∞" },
    { id: "poe",        name: "Poe",        url: "https://poe.com/",                  domains: ["poe.com"],                        color: "#8b5cf6", icon: "P" }
  ];

  // Don't put the switcher on the hub itself — the hub IS the switcher.
  if (location.hostname === "127.0.0.1" || location.hostname === "localhost") return;

  const here = AI.find(s => s.domains.some(d => location.hostname.endsWith(d)));

  /* The whole UI lives in a shadow root. AI sites have aggressive global CSS
   * and their own keyboard handling; a shadow root keeps their styles out of
   * this and this out of their layout. */
  const host = document.createElement("div");
  host.id = "ai-hub-switcher";
  host.style.cssText = "position:fixed;inset:0;pointer-events:none;z-index:2147483647;";
  const root = host.attachShadow({ mode: "closed" });

  root.innerHTML = `
    <style>
      :host { all: initial; }
      * { box-sizing: border-box; font-family: Inter, system-ui, -apple-system, "Segoe UI", sans-serif; }

      .fab {
        position: fixed; top: 12px; right: 14px;
        pointer-events: auto;
        display: flex; align-items: center; gap: 7px;
        padding: 7px 12px;
        border: 1px solid rgba(255,255,255,.14);
        border-radius: 10px;
        background: #11131a; color: #fff;
        font-size: 12.5px; font-weight: 700;
        cursor: pointer;
        box-shadow: 0 8px 26px rgba(0,0,0,.35);
        opacity: .55; transition: opacity .15s, transform .15s;
      }
      .fab:hover { opacity: 1; transform: translateY(-1px); }
      .fab .dot { width: 7px; height: 7px; border-radius: 50%; background: var(--current, #7c5cff); }

      .backdrop {
        position: fixed; inset: 0;
        pointer-events: auto;
        background: rgba(0,0,0,.45);
        display: none;
      }
      .backdrop.open { display: block; }

      .panel {
        position: fixed; top: 52px; right: 14px;
        width: 250px;
        pointer-events: auto;
        border: 1px solid rgba(255,255,255,.12);
        border-radius: 14px;
        background: #0d0f15;
        box-shadow: 0 24px 60px rgba(0,0,0,.55);
        overflow: hidden;
        display: none;
      }
      .panel.open { display: block; }

      .head {
        padding: 11px 13px;
        border-bottom: 1px solid rgba(255,255,255,.08);
        color: #858b9a; font-size: 11px;
        display: flex; justify-content: space-between;
      }

      .list { max-height: 62vh; overflow-y: auto; padding: 6px; }

      .item {
        display: flex; align-items: center; gap: 10px;
        padding: 8px 9px;
        border-radius: 9px;
        color: #f5f7fa; font-size: 13px;
        cursor: pointer;
        border: 1px solid transparent;
      }
      .item:hover, .item.sel { background: rgba(255,255,255,.07); border-color: rgba(255,255,255,.1); }
      .item.current { color: #858b9a; cursor: default; }
      .item .ico {
        width: 22px; height: 22px; flex: 0 0 22px;
        display: grid; place-items: center;
        border-radius: 6px; color: #fff;
        font-size: 12px; font-weight: 800;
      }
      .item .tag { margin-left: auto; color: #5b6170; font-size: 10px; }

      .foot { border-top: 1px solid rgba(255,255,255,.08); padding: 6px; }
      .hint { padding: 7px 13px 10px; color: #4e5462; font-size: 10px; }
      kbd {
        padding: 1px 5px; border: 1px solid rgba(255,255,255,.12);
        border-radius: 4px; background: rgba(255,255,255,.05); color: #9aa0ad;
      }
    </style>

    <button class="fab" part="fab"><span class="dot"></span>AI</button>
    <div class="backdrop"></div>
    <div class="panel">
      <div class="head"><span>Switch assistant</span><span>${here ? here.name : "AI"}</span></div>
      <div class="list"></div>
      <div class="foot"></div>
      <div class="hint"><kbd>alt</kbd>+<kbd>a</kbd> open · <kbd>↑↓</kbd> move · <kbd>↵</kbd> go · <kbd>h</kbd> hub</div>
    </div>`;

  const fab = root.querySelector(".fab");
  const panel = root.querySelector(".panel");
  const backdrop = root.querySelector(".backdrop");
  const list = root.querySelector(".list");
  const foot = root.querySelector(".foot");

  if (here) fab.style.setProperty("--current", here.color);

  const rows = [];

  function addRow(parent, { label, icon, color, tag, current, go }) {
    const el = document.createElement("div");
    el.className = "item" + (current ? " current" : "");
    el.innerHTML = `<span class="ico" style="background:${color}">${icon}</span><span>${label}</span>` +
                   (tag ? `<span class="tag">${tag}</span>` : "");
    parent.appendChild(el);
    if (current) return;
    el.addEventListener("click", go);
    el.addEventListener("mouseenter", () => highlight(rows.indexOf(entry)));
    const entry = { el, go };
    rows.push(entry);
  }

  for (const s of AI) {
    const current = here && s.id === here.id;
    addRow(list, {
      label: s.name, icon: s.icon, color: s.color,
      tag: current ? "current" : "",
      current,
      go: () => { window.open(s.url, "_blank", "noopener"); close(); }
    });
  }

  addRow(foot, {
    label: "Open AI Hub", icon: "✦", color: "#7c5cff", tag: "h",
    go: () => { window.open(HUB_URL, "_blank", "noopener"); close(); }
  });

  let sel = 0;

  function highlight(i) {
    if (!rows.length) return;
    sel = (i + rows.length) % rows.length;
    rows.forEach((r, n) => r.el.classList.toggle("sel", n === sel));
    rows[sel].el.scrollIntoView({ block: "nearest" });
  }

  function open() {
    panel.classList.add("open");
    backdrop.classList.add("open");
    highlight(0);
  }

  function close() {
    panel.classList.remove("open");
    backdrop.classList.remove("open");
  }

  function isOpen() { return panel.classList.contains("open"); }

  fab.addEventListener("click", () => (isOpen() ? close() : open()));
  backdrop.addEventListener("click", close);

  // Capture phase: chatbot pages bind their own global key handlers, and
  // several of them stop propagation. Listening on the way down means the
  // switcher still gets alt+a and Escape.
  window.addEventListener("keydown", e => {
    if (e.altKey && !e.ctrlKey && !e.metaKey && (e.key === "a" || e.key === "A")) {
      e.preventDefault(); e.stopPropagation();
      isOpen() ? close() : open();
      return;
    }
    if (!isOpen()) return;

    if (e.key === "Escape")     { e.preventDefault(); e.stopPropagation(); close(); return; }
    if (e.key === "ArrowDown")  { e.preventDefault(); e.stopPropagation(); highlight(sel + 1); return; }
    if (e.key === "ArrowUp")    { e.preventDefault(); e.stopPropagation(); highlight(sel - 1); return; }
    if (e.key === "Enter")      { e.preventDefault(); e.stopPropagation(); rows[sel].go(); return; }
    if (e.key === "h" || e.key === "H") {
      e.preventDefault(); e.stopPropagation();
      rows[rows.length - 1].go();
    }
  }, true);

  (document.body || document.documentElement).appendChild(host);
})();
