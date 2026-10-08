/* AI Hub — a keyboard-first launcher for AI assistants.
 *
 * Everything it knows comes from data/ai.json. Adding an assistant means
 * adding one object there; nothing in this file lists them.
 *
 * Two rules the layout obeys:
 *   - the page never scrolls. The grid's rows and columns are computed from
 *     how many cards are visible, so they always fill the viewport exactly.
 *   - arrow keys read the REAL column count back out of the grid, rather than
 *     assuming 4. Otherwise navigation silently breaks whenever the layout
 *     changes.
 */

const FAV_KEY = "ai-hub.favorites";

const grid = document.getElementById("grid");
const search = document.getElementById("search");
const filterBar = document.getElementById("filters");
const statusEl = document.getElementById("status");
const countEl = document.getElementById("count");

let services = [];
let visible = [];
let selected = 0;
let filter = "all";
let favorites = loadFavorites();

/* ---------- favourites (localStorage, no backend) ---------- */

function loadFavorites() {
  try {
    const raw = localStorage.getItem(FAV_KEY);
    return raw ? new Set(JSON.parse(raw)) : null;
  } catch (e) {
    return null;   // private window, or storage disabled — fall back to the file
  }
}

function saveFavorites() {
  try {
    localStorage.setItem(FAV_KEY, JSON.stringify([...favorites]));
  } catch (e) { /* not worth breaking the page over */ }
}

function isFav(s) {
  return favorites ? favorites.has(s.id) : !!s.favorite;
}

function toggleFav(s) {
  if (!favorites) favorites = new Set(services.filter(x => x.favorite).map(x => x.id));
  favorites.has(s.id) ? favorites.delete(s.id) : favorites.add(s.id);
  saveFavorites();
  render();
}

/* ---------- data ---------- */

async function load() {
  try {
    const res = await fetch("data/ai.json", { cache: "no-cache" });
    if (!res.ok) throw new Error("HTTP " + res.status);
    services = await res.json();
    statusEl.textContent = "ready";
  } catch (e) {
    // The usual cause is opening index.html as a file:// URL — fetch is blocked
    // there by the browser, not by anything in this project. bin/ai-hub serves
    // the page over http://127.0.0.1 for exactly this reason.
    statusEl.textContent = "could not load data/ai.json — serve this page over http (bin/ai-hub)";
    grid.innerHTML = '<div class="empty">data/ai.json did not load.<br>Open this page through <code>bin/ai-hub</code>, not as a file:// URL.</div>';
    return;
  }

  buildFilters();
  render();
}

function buildFilters() {
  const cats = ["all", "favorites", ...new Set(services.flatMap(s => s.category))];
  filterBar.innerHTML = "";
  for (const c of cats) {
    const b = document.createElement("button");
    b.className = "filter" + (c === filter ? " active" : "");
    b.textContent = c === "all" ? "All" : c === "favorites" ? "★ Favourites" : c[0].toUpperCase() + c.slice(1);
    b.dataset.filter = c;
    b.addEventListener("click", () => { filter = c; selected = 0; render(); });
    filterBar.appendChild(b);
  }
}

/* ---------- render ---------- */

function matches(s) {
  const q = search.value.trim().toLowerCase();
  const hay = (s.name + " " + s.provider + " " + s.id + " " + s.category.join(" ")).toLowerCase();
  if (q && !hay.includes(q)) return false;
  if (filter === "all") return true;
  if (filter === "favorites") return isFav(s);
  return s.category.includes(filter);
}

function layout(n) {
  // Choose rows and columns so the grid fills the viewport exactly. Two things
  // are traded off:
  //   - card shape, aimed at a landscape-ish card (ratio ~1.6);
  //   - holes: a 5x3 grid for 12 cards leaves three empty cells in the last
  //     row and looks unfinished, while 4x3 is flush. The penalty is what makes
  //     12 items land on 4x3 instead of 5x3.
  if (n === 0) return [1, 1];
  const w = grid.clientWidth || 1200;
  const h = grid.clientHeight || 600;
  let best = [Math.min(4, n), Math.ceil(n / Math.min(4, n))];
  let bestScore = Infinity;
  for (let cols = 1; cols <= Math.min(6, n); cols++) {
    const rows = Math.ceil(n / cols);
    const ratio = (w / cols) / (h / rows);
    const shape = Math.abs(Math.log(ratio / 1.6));
    const holes = (cols * rows - n) / n;
    const score = shape + 1.6 * holes;
    if (score < bestScore) { bestScore = score; best = [cols, rows]; }
  }
  return best;
}

function render() {
  for (const b of filterBar.children) b.classList.toggle("active", b.dataset.filter === filter);

  visible = services.filter(matches);
  const [cols, rows] = layout(visible.length);
  grid.style.gridTemplateColumns = `repeat(${cols}, minmax(0, 1fr))`;
  grid.style.gridTemplateRows = `repeat(${rows}, minmax(0, 1fr))`;

  grid.innerHTML = "";
  if (!visible.length) {
    grid.innerHTML = '<div class="empty">Nothing matches.</div>';
    countEl.textContent = "0 of " + services.length;
    return;
  }

  visible.forEach((s, i) => {
    const a = document.createElement("a");
    a.className = "card" + (isFav(s) ? " fav" : "");
    a.href = s.url;
    a.target = "_blank";
    a.rel = "noopener";
    a.style.setProperty("--brand", s.color);
    a.innerHTML =
      `<div class="icon">${s.icon}</div>` +
      `<span class="star">${isFav(s) ? "★" : "☆"}</span>` +
      `<h3>${s.name}</h3>` +
      `<div class="provider">${s.provider}</div>` +
      `<p>${s.blurb}</p>` +
      `<div class="open">Open<span class="arrow">↗</span></div>`;
    a.addEventListener("mouseenter", () => select(i));
    a.addEventListener("click", ev => launch(s, ev));
    a.querySelector(".star").addEventListener("click", ev => {
      ev.preventDefault(); ev.stopPropagation();
      toggleFav(s);
    });
    grid.appendChild(a);
  });

  select(Math.min(selected, visible.length - 1));
  countEl.textContent = visible.length === services.length
    ? services.length + " assistants"
    : visible.length + " of " + services.length;
}

/* Opening an assistant.
 *
 * The href is a real link, so middle-click, ctrl-click and a statically hosted
 * copy of this page all still work. But a plain left click is intercepted and
 * handed to the local server instead, because a browser turns target="_blank"
 * from an app window into a TAB in whatever ordinary browser window already
 * exists — on whatever workspace that window is on. No window is created, so no
 * window-manager rule can place it. /open?id= asks the server to start the
 * assistant as its own application window, which i3 can then assign.
 *
 * If the server is not there (page opened from a static host), nothing is
 * intercepted and the link behaves normally.
 */
function launch(service, ev) {
  if (ev.defaultPrevented) return;
  if (ev.button !== 0 || ev.ctrlKey || ev.shiftKey || ev.metaKey || ev.altKey) return;
  ev.preventDefault();
  fetch("open?id=" + encodeURIComponent(service.id))
    .then(r => { if (!r.ok) throw new Error("HTTP " + r.status); })
    .catch(() => window.open(service.url, "_blank", "noopener"));
}

function select(i) {
  if (!visible.length) return;
  selected = Math.max(0, Math.min(i, visible.length - 1));
  [...grid.children].forEach((c, n) => c.classList.toggle("selected", n === selected));
}

function columns() {
  // Read the real column count back out of the grid instead of assuming one.
  return grid.style.gridTemplateColumns.split("repeat(")[1]?.split(",")[0] * 1 || 1;
}

/* ---------- keyboard ---------- */

search.addEventListener("input", () => { selected = 0; render(); });

document.addEventListener("keydown", e => {
  const typing = document.activeElement === search;

  if (e.key === "/" && !typing) { e.preventDefault(); search.focus(); search.select(); return; }

  if (e.key === "Escape") {
    if (search.value) { search.value = ""; selected = 0; render(); }
    search.blur();
    return;
  }

  if (e.key === "Enter") {
    e.preventDefault();
    if (visible.length) grid.children[selected].click();
    return;
  }

  // While typing, only the arrows are borrowed — every other key belongs to the
  // search box.
  const arrows = ["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown"];
  if (typing && !arrows.includes(e.key)) return;

  if (!typing && (e.key === "f" || e.key === "F")) {
    e.preventDefault();
    if (visible.length) toggleFav(visible[selected]);
    return;
  }

  if (!arrows.includes(e.key)) return;
  e.preventDefault();

  const cols = columns();
  const delta = { ArrowLeft: -1, ArrowRight: 1, ArrowUp: -cols, ArrowDown: cols }[e.key];
  select(selected + delta);
});

window.addEventListener("resize", render);
load();
