/* The in-game phone on the website: the game's own phone, its home screen, apps and colours, to tap through.
 * The look comes from the game (game-phone.css); the content is a short example of a working day at the airport. */
(function () {
  "use strict";
  // #region ── Icons, money and small helpers (as in the game) ──
  var IC = {"mail": "<rect x=\"3\" y=\"5.5\" width=\"18\" height=\"13\" rx=\"2\"/><path d=\"M3.5 7l8.5 6.2L20.5 7\"/>", "calendar": "<rect x=\"3.5\" y=\"5\" width=\"17\" height=\"15.5\" rx=\"2\"/><path d=\"M3.5 9.5h17M8 3v4M16 3v4\"/><path d=\"M7.5 13h2M11 13h2M14.5 13h2M7.5 16.5h2M11 16.5h2\"/>", "bank": "<path d=\"M3 9.5L12 4l9 5.5\"/><path d=\"M4.5 10v8M9 10v8M15 10v8M19.5 10v8M3 20h18\"/>", "news": "<path d=\"M4 5h13v14H6a2 2 0 0 1-2-2z\"/><path d=\"M17 8h3v9a2 2 0 0 1-2 2\"/><path d=\"M7 8h7M7 11.5h7M7 15h4\"/>", "camera": "<path d=\"M4 8h3.2L9 5.5h6L16.8 8H20a1 1 0 0 1 1 1v9.5a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V9a1 1 0 0 1 1-1z\"/><circle cx=\"12\" cy=\"13.5\" r=\"3.6\"/>", "cinema": "<circle cx=\"7\" cy=\"6.2\" r=\"2.6\"/><circle cx=\"13.2\" cy=\"6.2\" r=\"2.6\"/><rect x=\"3\" y=\"10\" width=\"13\" height=\"8.5\" rx=\"1.4\"/><path d=\"M16 12.6l5-2.6v8.5l-5-2.6\"/><path d=\"M6.5 18.5l-1.2 3M12.5 18.5l1.2 3\"/>", "trophy": "<path d=\"M8 4h8v5a4 4 0 0 1-8 0z\"/><path d=\"M8 6H5a3 3 0 0 0 3 4M16 6h3a3 3 0 0 1-3 4\"/><path d=\"M12 13v4M8.5 20h7M9.5 17h5\"/>", "user": "<circle cx=\"12\" cy=\"8.5\" r=\"3.8\"/><path d=\"M4.5 20.5c1-4 4-6 7.5-6s6.5 2 7.5 6\"/>", "fuel": "<path d=\"M5 20.5V5a1.5 1.5 0 0 1 1.5-1.5h6A1.5 1.5 0 0 1 14 5v15.5M3.5 20.5h12M5 10h9\"/><path d=\"M14 8.5h2.2a1.3 1.3 0 0 1 1.3 1.3v6.4a1.4 1.4 0 0 0 2.8 0V7.5L18 5\"/>", "market": "<path d=\"M3.5 11.5V4.5a1 1 0 0 1 1-1h7l9 9-8 8-9-9z\"/><circle cx=\"8\" cy=\"8\" r=\"1.6\"/>", "globe": "<circle cx=\"12\" cy=\"12\" r=\"8.5\"/><path d=\"M3.5 12h17M12 3.5c2.4 2.3 3.6 5.1 3.6 8.5s-1.2 6.2-3.6 8.5c-2.4-2.3-3.6-5.1-3.6-8.5s1.2-6.2 3.6-8.5z\"/>", "gear": "<circle cx=\"12\" cy=\"12\" r=\"3\"/><path d=\"M12 2.8v2.4M12 18.8v2.4M2.8 12h2.4M18.8 12h2.4M5.5 5.5l1.7 1.7M16.8 16.8l1.7 1.7M5.5 18.5l1.7-1.7M16.8 7.2l1.7-1.7\"/><circle cx=\"12\" cy=\"12\" r=\"6.3\"/>", "chat": "<path d=\"M12 4c4.7 0 8.5 3.1 8.5 7s-3.8 7-8.5 7c-1 0-2-.1-2.9-.4L5 19.5l1.2-3.4C4.6 14.8 3.5 13 3.5 11c0-3.9 3.8-7 8.5-7z\"/>", "phone": "<path d=\"M5.5 3.5h3l1.6 4.2-2 1.3a11 11 0 0 0 6.9 6.9l1.3-2 4.2 1.6v3a2 2 0 0 1-2.1 2A16.5 16.5 0 0 1 3.5 5.6a2 2 0 0 1 2-2.1z\"/>", "store": "<path d=\"M5 8h14l-1.2 11.2a1.5 1.5 0 0 1-1.5 1.3H7.7a1.5 1.5 0 0 1-1.5-1.3z\"/><path d=\"M9 10.5V7a3 3 0 0 1 6 0v3.5\"/>", "close": "<path d=\"M6 6l12 12M18 6 6 18\"/>", "back": "<path d=\"M14.5 5.5L8 12l6.5 6.5\"/>", "chevron": "<path d=\"M9 5.5 15.5 12 9 18.5\"/>", "plus": "<path d=\"M12 5v14M5 12h14\"/>", "shield": "<path d=\"M12 3 19.5 6v5.5c0 4.6-3.2 8.3-7.5 9.5-4.3-1.2-7.5-4.9-7.5-9.5V6z\"/>", "star": "<path d=\"M12 3.6l2.6 5.3 5.8.8-4.2 4.1 1 5.8L12 16.9l-5.2 2.7 1-5.8-4.2-4.1 5.8-.8z\"/>", "clock": "<circle cx=\"12\" cy=\"12\" r=\"8.5\"/><path d=\"M12 7v5.2l3.4 2\"/>", "sliders": "<path d=\"M4 7h10M18 7h2M4 17h4M12 17h8\"/><circle cx=\"16\" cy=\"7\" r=\"2\"/><circle cx=\"10\" cy=\"17\" r=\"2\"/>", "info": "<circle cx=\"12\" cy=\"12\" r=\"8.5\"/><path d=\"M12 11v5.5M12 7.6v.1\"/>", "trash": "<path d=\"M4.5 7h15M9.5 7V4.8a.8.8 0 0 1 .8-.8h3.4a.8.8 0 0 1 .8.8V7M6.5 7l.9 12.2a1.5 1.5 0 0 0 1.5 1.3h6.2a1.5 1.5 0 0 0 1.5-1.3L17.5 7M10.2 11v5.5M13.8 11v5.5\"/>", "check": "<path d=\"M4.5 12.5 9.5 17.5 19.5 7\"/>", "map": "<circle cx=\"12\" cy=\"12\" r=\"8.5\"/><path d=\"M3.5 12h17M12 3.5c2.6 2.4 3.8 5.2 3.8 8.5s-1.2 6.1-3.8 8.5c-2.6-2.4-3.8-5.2-3.8-8.5S9.4 5.9 12 3.5z\"/>", "wind": "<path d=\"M3 8.5h10.5a2.5 2.5 0 1 0-2.4-3.2\"/><path d=\"M3 12.5h15a3 3 0 1 1-2.9 3.8\"/><path d=\"M3 16.5h7\"/>", "mute": "<path d=\"M4 9.5h3.5L12 5.5v13l-4.5-4H4z\"/><path d=\"M16 9.5l5 5M21 9.5l-5 5\"/>", "volume": "<path d=\"M4 9.5h3.5L12 5.5v13l-4.5-4H4z\"/><path d=\"M15.5 9a4.2 4.2 0 0 1 0 6M18 6.5a8 8 0 0 1 0 11\"/>", "layers": "<path d=\"M12 3.5 21 8.5l-9 5-9-5z\"/><path d=\"M3 12.5l9 5 9-5M3 16.5l9 5 9-5\"/>", "alert": "<path d=\"M12 3.5 21.5 20h-19z\"/><path d=\"M12 10v4.5M12 17.4v.1\"/>"};
  function icon(n) { return '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">' + (IC[n] || "") + "</svg>"; }
  function esc(s) { return String(s).replace(/[&<>"]/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]; }); }
  function money(n) { var a = Math.round(Math.abs(n)).toLocaleString("en-US"); return (n < 0 ? "−" : "") + "$" + a; }
  function hm(d) { return d.toTimeString().slice(0, 5); }
  function ago(min) { return new Date(Date.now() - min * 60000); }
  function day(d) { return d.toLocaleDateString("en-GB", { day: "numeric", month: "short" }); }
  // #endregion

  // #region ── The example: the airport, its people, its mail and its money ──
  var CO = { name: "Mýrdalssandur Airport", color: "#2C8CFF", color2: "#0B2A55" };
  var BANK = "Nordlys Bank";
  var APPS = [
    ["mail", "Mail", "mail", "#3FA2F0"], ["calendar", "Calendar", "calendar", "#F06A6A"], ["bank", "Bank", "bank", "#2FBF8F"], ["docs", "My Documents", "news", "#4F8CF6"], ["photos", "Photos", "camera", "#B07CF0"], ["videos", "Videos", "cinema", "#E2557A"], ["recorder", "Recorder", "cinema", "#F0546E"],
    ["ranking", "Ranking", "trophy", "#F5B544"], ["career", "My career", "user", "#3FD0E8"], ["markets", "Markets", "fuel", "#E98B3A"], ["acmarket", "Aircraft market", "market", "#7C8CF0"], ["browser", "Web browser", "globe", "#5BC0EB"], ["settings", "Settings", "gear", "#8A97A8"]
  ];
  var PH_APPS = [["msgs", "Messages", "chat", "#34C759"], ["call", "Phone", "phone", "#30B158"], ["store", "Store", "store", "#2F7BF6"]];
  var ALL = APPS.concat(PH_APPS);
  var C = [
    { id: "w", name: "Anna", sub: "Wife", fam: true, c: "#F06A9A" }, { id: "d", name: "Sóley", sub: "Daughter", fam: true, c: "#F06A9A" }, { id: "m", name: "Mum", sub: "Mother", fam: true, c: "#F06A9A" },
    { id: "s1", name: "Capt. Jónas Berg", sub: "Captain", fam: false, c: "#3FA2F0" }, { id: "s2", name: "Elín Dahl", sub: "Duty manager", fam: false, c: "#3FA2F0" },
    { id: "s3", name: "Mark Olsen", sub: "Head of security", fam: false, c: "#3FA2F0" }, { id: "s4", name: "Rúna Hansen", sub: "Fire chief", fam: false, c: "#3FA2F0" }, { id: "s5", name: "Tom Ellis", sub: "Chief engineer", fam: false, c: "#3FA2F0" }
  ];
  function contact(id) { for (var i = 0; i < C.length; i++) if (C[i].id === id) return C[i]; return null; }
  function ini(n) { return n.replace(/^(Capt\.)\s+/, "").split(/\s+/).slice(0, 2).map(function (w) { return w.charAt(0); }).join("").toUpperCase(); }
  function av(c, big) { return '<span class="pa-av ' + (big ? "big" : "") + '" style="--c:' + c.c + '">' + esc(ini(c.name)) + "</span>"; }
  var MAIL = [
    { id: "1", from: "Elín Dahl (Duty manager)", addr: "elin.dahl@myrdalssandur.example", kind: "note", subject: "Fuel delivery moved to Thursday", body: "Hi,\n\nThe fuel supplier has moved this week's jet fuel delivery to Thursday morning. We have enough in the tanks until then, but I'd like to order a little extra for the weekend rush.\n\nShall I go ahead?\n\nElín", at: 25 },
    { id: "2", from: "Nordlys Bank", addr: "noreply@nordlys.example", kind: "bank", subject: "Your salary has been paid", body: "Your monthly salary has been paid into your personal account.\n\nLog in to the Bank app to see your balance and every transaction.", at: 190 },
    { id: "3", from: "Westbound Air", addr: "ops@westbound.example", kind: "lease", subject: "New route: three flights a week", body: "Dear airport manager,\n\nWe would like to start three flights a week to your airport from next month, with a 180-seat jet.\n\nWe look forward to your landing fees and stand prices.", at: 420 },
    { id: "4", from: "Tom Ellis (Chief engineer)", addr: "tom.ellis@myrdalssandur.example", kind: "report", subject: "A-check due on stand 3's aircraft", body: "The aircraft on stand 3 is due its A-check this week. Parts are ordered; the hangar is free on Wednesday night.", at: 1500 },
    { id: "5", from: "Anna (your wife)", addr: "home@mail.example", kind: "family", subject: "Dinner tonight?", body: "Are you home for dinner tonight, or is it another late shift at the airport? 😊", at: 1720 }
  ];
  var KIND_IC = { court: "shield", report: "alert", lease: "layers", family: "user", bank: "bank", note: "calendar" };
  var MSGS = {
    w: [{ me: false, t: "Don't forget Sóley's school play at six!", at: 140 }, { me: true, t: "I'll be there. Leaving the terminal at five.", at: 132 }, { me: false, t: "❤️", at: 130 }],
    s2: [{ me: false, t: "Gate 2's jetway is fixed. Back in use from the next arrival.", at: 48 }, { me: true, t: "Great work, thanks Elín.", at: 45 }],
    s3: [{ me: false, t: "Security queue is down to four minutes.", at: 15 }]
  };
  var CALLS = [{ id: "s4", dir: "in", ok: true, dur: 134, at: 70 }, { id: "w", dir: "out", ok: true, dur: 312, at: 240 }, { id: "s5", dir: "in", ok: false, at: 600 }];
  var COMPANY_TX = [
    ["Landing fees · Westbound Air", 18400], ["Staff salaries · November", -96250], ["Shop and restaurant rent", 24800], ["Jet fuel · 42,000 litres", -38900], ["Parking and car rental", 9650],
    ["Aircraft maintenance · A-check", -27300], ["Goods for the terminal's shops", -14200], ["Passenger fees", 31700], ["Food, drinks and supplies for the cabins", -8750], ["Stand fees", 12100]
  ];
  var PERSONAL_TX = [["Salary · Mýrdalssandur Airport", 7800], ["Groceries", -164], ["Petrol", -92], ["Hairdresser", -45], ["Electricity · the villa", -138]];
  var Acct = { main: 1284500, own: 23460 };
  var NOTES = {};
  // #endregion

  // #region ── The phone's state and drawing ──
  var P = { app: "", mailId: "", bankIn: false, bankBusy: false, bankTab: "main", bankFil: "all", thread: "", typing: "", tab: "recents", dial: "", calling: null, callState: "", calDay: "" };
  var root = null, frame = null, PHOTOS = [];

  function now() { return new Date(); }
  function ymd(d) { return d.getFullYear() + "-" + String(d.getMonth() + 1).padStart(2, "0") + "-" + String(d.getDate()).padStart(2, "0"); }
  function unread() { return MAIL.filter(function (m) { return !m.read; }).length; }
  function msgsUnread() { var n = 0; for (var k in MSGS) MSGS[k].forEach(function (m) { if (!m.me && !m.read) n++; }); return n; }

  function statusBar() {
    var un = unread();
    return '<b class="ph-time">' + hm(now()) + '</b><span class="ph-notch"></span><span class="ph-ind">' + (un ? '<i class="ph-dot">' + icon("mail") + "</i>" : "") +
      '<svg viewBox="0 0 18 12" aria-hidden="true"><rect x="0" y="8" width="3" height="4" rx="1"/><rect x="5" y="5" width="3" height="7" rx="1"/><rect x="10" y="2.5" width="3" height="9.5" rx="1"/><rect x="15" y="0" width="3" height="12" rx="1"/></svg><span class="ph-bat"><i style="width:78%"></i></span></span>';
  }
  function deskHtml() {
    var d = now(), un = unread(), t = ymd(d), notes = Object.keys(NOTES).filter(function (k) { return k >= t; }).sort().slice(0, 3);
    return '<input class="lb-search ph-search" placeholder="Search" aria-label="Search apps and files" autocomplete="off" readonly><div class="ld-wall" style="--c1:' + CO.color + ";--c2:" + CO.color2 + '"><span class="ld-brand">' + esc(CO.name) + "</span></div>" +
      '<div class="ld-icons">' + ALL.map(function (a) { return '<button class="ld-ic" data-act="app" data-k="' + a[0] + '"><span style="--c:' + a[3] + '">' + icon(a[2]) + (a[0] === "mail" && un ? '<i class="lb-badge">' + un + "</i>" : "") + "</span><b>" + esc(a[1]) + "</b></button>"; }).join("") + "</div>" +
      '<div class="ld-widget"><b class="ld-time">' + hm(d) + "</b><small>" + esc(d.toLocaleDateString("en-GB", { weekday: "long", day: "numeric", month: "long", year: "numeric" })) + "</small>" +
      '<div class="ld-notes">' + (notes.length ? notes.map(function (k) { return "<span><i>" + k.slice(5).split("-").reverse().join("/") + "</i>" + esc(NOTES[k][0]) + "</span>"; }).join("") : '<span class="muted">Nothing in your calendar</span>') + "</div>" +
      "<small>" + (un ? un + " unread " + (un === 1 ? "message" : "messages") : "No new mail") + "</small></div>";
  }
  function dockHtml() {
    var un = msgsUnread();
    return '<div class="ph-dock-in">' + PH_APPS.map(function (a) { return '<button class="ld-ic pd-ic" data-act="app" data-k="' + a[0] + '" aria-label="' + esc(a[1]) + '"><span style="--c:' + a[3] + '">' + icon(a[2]) + (a[0] === "msgs" && un ? '<i class="lb-badge">' + un + "</i>" : "") + "</span></button>"; }).join("") + "</div>";
  }
  function render() {
    if (!root) return;
    root.querySelector(".ph-status").innerHTML = statusBar();
    root.querySelector(".lap-desk").innerHTML = deskHtml();
    root.querySelector(".ph-dock").innerHTML = dockHtml();
    var win = root.querySelector(".lap-win");
    if (!P.app) { win.hidden = true; win.innerHTML = ""; return; }
    var A = ALL.filter(function (x) { return x[0] === P.app; })[0];
    win.hidden = false;
    win.innerHTML = '<div class="lap-win-h"><span class="lw-ic" style="--c:' + A[3] + '">' + icon(A[2]) + "</span><b>" + esc(A[1]) + '</b><span class="lw-sp"></span><button class="lw-x" data-act="app" data-k="" aria-label="Close ' + esc(A[1]) + '">' + icon("close") + '</button></div><div class="lap-win-b lap-app-' + P.app + '">' + appHtml(P.app) + "</div>";
    var l = win.querySelector(".pm-list"); if (l) l.scrollTop = l.scrollHeight;
  }
  // #endregion

  // #region ── The apps ──
  function appHtml(k) {
    if (k === "mail") return mailHtml();
    if (k === "bank") return bankHtml();
    if (k === "calendar") return calHtml();
    if (k === "msgs") return msgsHtml();
    if (k === "call") return callHtml();
    if (k === "store") return storeHtml();
    if (k === "markets") return marketsHtml();
    if (k === "photos") return photosHtml();
    var T = {
      docs: ["news", "My Documents", "Contracts, staff files, airline agreements and the airport's plans, in folders of your own."],
      videos: ["cinema", "Videos", "Clips you recorded at the airport: landings at sunset, the fire crew's drills and more."],
      recorder: ["cinema", "Recorder", "Record what happens in the game, then watch it back or save it."],
      ranking: ["trophy", "Ranking", "How your airport stands against the others: passengers, punctuality, safety and service."],
      career: ["user", "My career", "Your rank, your awards and everything you have built so far."],
      acmarket: ["market", "Aircraft market", "New and used aircraft to buy or lease, with their prices and running costs."],
      browser: ["globe", "Web browser", "Real web pages, right inside the game."],
      settings: ["gear", "Settings", "Wallpaper, sounds, the clock and your phone's other settings."]
    }[k] || ["info", k, ""];
    return '<div class="adp-info"><span>' + icon(T[0]) + "</span><b>" + esc(T[1]) + "</b><p>" + esc(T[2]) + "</p></div>";
  }

  function mailHtml() {
    var cur = MAIL.filter(function (m) { return m.id === P.mailId; })[0];
    var folders = '<div class="lm-boxes">' + [["", "Inbox", "mail"], ["star", "Starred", "star"], ["spam", "Spam", "alert"], ["deleted", "Deleted", "trash"]].map(function (b, i) { var n = i === 0 ? unread() : 0; return '<button class="lm-box ' + (i === 0 ? "on" : "") + '">' + icon(b[2]) + "<span>" + b[1] + "</span>" + (n ? "<em>" + n + "</em>" : "") + "</button>"; }).join("") + "</div>";
    var list = MAIL.map(function (m) { return '<button class="lm-row ' + (m.read ? "" : "new") + " " + (cur === m ? "on" : "") + '" data-act="mail" data-id="' + m.id + '"><span class="lm-ic k-' + m.kind + '">' + icon(KIND_IC[m.kind] || "mail") + '</span><span class="lm-t"><b>' + esc(m.from) + "</b><span>" + esc(m.subject) + "</span><small>" + (m.at < 1440 ? hm(ago(m.at)) : day(ago(m.at))) + "</small></span></button>"; }).join("");
    var view = cur ? '<div class="lm-view"><div class="lm-vh"><button class="lm-back" data-act="mail-back">' + icon("back") + "Inbox</button><h3>" + esc(cur.subject) + "</h3><small><b>" + esc(cur.from) + "</b> &lt;" + esc(cur.addr) + "&gt;</small></div>" +
      '<div class="lm-body">' + esc(cur.body).replace(/\n/g, "<br>") + "</div></div>" : '<div class="lm-view lm-none"><span>' + icon("mail") + "</span><p>Choose a message.</p></div>";
    return '<div class="lm lm3 ' + (cur ? "lm-open" : "") + '">' + folders + '<div class="lm-list"><div class="lm-lh"><b>Inbox</b><small>' + unread() + " unread</small></div>" + list + "</div>" + view + "</div>";
  }

  function bankHtml() {
    if (!P.bankIn) return '<div class="lk-login ' + (P.bankBusy ? "busy" : "") + '"><div class="lk-lbg"></div><div class="lk-lcard"><div class="lk-logo"><span>' + icon("bank") + "</span><b>" + BANK + "</b><small>Internet bank</small></div>" +
      '<label class="lk-f"><small>Customer</small><span>' + esc(CO.name) + '</span></label><label class="lk-f"><small>Password</small><span class="lk-dots">' + "<i></i>".repeat(9) + "</span></label>" +
      '<button class="lk-go" data-act="bank-login">' + (P.bankBusy ? '<i class="lk-spin"></i>Logging in…' : "Log in") + "</button><p>" + icon("shield") + "Secure connection · " + BANK + "</p></div></div>";
    var tab = P.bankTab, fil = P.bankFil, src = tab === "main" ? COMPANY_TX : PERSONAL_TX;
    var tx = src.map(function (t, i) { return { label: t[0], amount: t[1], at: ago(i * 410 + 30) }; });
    var list = tx.filter(function (t) { return fil === "all" || (fil === "in" ? t.amount > 0 : t.amount < 0); });
    var inM = tx.filter(function (t) { return t.amount > 0; }).reduce(function (s, t) { return s + t.amount; }, 0), outM = tx.filter(function (t) { return t.amount < 0; }).reduce(function (s, t) { return s + t.amount; }, 0);
    var accts = '<div class="lk-accts"><button class="lk-acct ' + (tab === "main" ? "on" : "") + '" data-act="bank-tab" data-k="main"><small>Company account</small><b>' + money(Acct.main) + "</b><i>The airport pays and is paid here</i></button>" +
      '<button class="lk-acct ' + (tab === "own" ? "on" : "") + '" data-act="bank-tab" data-k="own"><small>Personal</small><b>' + money(Acct.own) + "</b><i>Your own account: your salary</i></button></div>";
    var main = '<div class="lk-sum"><div><small>In, last 30 days</small><b class="green">' + money(inM) + '</b></div><div><small>Out, last 30 days</small><b class="red">' + money(outM) + "</b></div></div>" +
      '<div class="lk-txh"><b>Transactions</b><span class="seg">' + [["all", "All"], ["in", "In"], ["out", "Out"]].map(function (f) { return '<button class="' + (fil === f[0] ? "on" : "") + '" data-act="bank-fil" data-k="' + f[0] + '">' + f[1] + "</button>"; }).join("") + "</span></div>" +
      '<div class="lk-tx">' + list.map(function (t) { return '<div class="lk-t"><span><b>' + esc(t.label) + "</b><small>" + day(t.at) + " " + hm(t.at) + '</small></span><b class="' + (t.amount < 0 ? "red" : "green") + '">' + (t.amount > 0 ? "+" : "") + money(t.amount) + "</b></div>"; }).join("") + "</div>";
    return '<div class="lk lk-in"><div class="lk-h"><b>' + icon("bank") + BANK + "</b><small>" + esc(CO.name) + '</small><span class="lw-sp"></span><button class="btn ghost sm" data-act="bank-out">Log out</button></div>' + accts + '<div class="lk-main">' + main + "</div></div>";
  }

  function calHtml() {
    var d = now(), today = ymd(d), y = d.getFullYear(), mo = d.getMonth(), first = new Date(y, mo, 1), days = new Date(y, mo + 1, 0).getDate(), lead = (first.getDay() + 6) % 7, sel = P.calDay || today, cells = [];
    for (var i = 0; i < lead; i++) cells.push('<span class="lc-d off"></span>');
    for (var k = 1; k <= days; k++) {
      var key = y + "-" + String(mo + 1).padStart(2, "0") + "-" + String(k).padStart(2, "0"), n = NOTES[key] || [];
      cells.push('<button class="lc-d ' + (key === today ? "today" : "") + " " + (key === sel ? "on" : "") + '" data-act="cal-day" data-d="' + key + '"><b>' + k + "</b>" + n.slice(0, 2).map(function (x) { return "<i>" + esc(x) + "</i>"; }).join("") + "</button>");
    }
    var dn = NOTES[sel] || [];
    return '<div class="lc"><div class="lc-main"><div class="lc-h"><b>' + esc(first.toLocaleDateString("en-GB", { month: "long", year: "numeric" })) + '</b></div><div class="lc-wd">' + ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"].map(function (x) { return "<span>" + x + "</span>"; }).join("") + '</div><div class="lc-grid">' + cells.join("") + "</div></div>" +
      '<div class="lc-side"><b>' + esc(new Date(sel + "T12:00:00").toLocaleDateString("en-GB", { weekday: "long", day: "numeric", month: "long" })) + "</b>" + (dn.map(function (t) { return '<div class="lc-n"><span>' + esc(t) + "</span></div>"; }).join("") || '<p class="muted">Nothing noted.</p>') +
      '<form class="lc-add" data-form="cal"><input class="le-in lc-in" maxlength="80" placeholder="Something to remember on this day"><button class="btn primary sm" type="submit">' + icon("plus") + "Add</button></form></div></div>";
  }

  function msgsHtml() {
    if (P.thread) {
      var c = contact(P.thread), L = MSGS[c.id] || [];
      L.forEach(function (m) { if (!m.me) m.read = true; });
      var rows = L.map(function (m) { return '<div class="pm-b ' + (m.me ? "me" : "") + '">' + esc(m.t) + "</div>"; }).join("");
      return '<div class="pm-thread"><div class="pm-th"><button class="lm-back" data-act="msg-back">' + icon("back") + "</button>" + av(c) + "<span><b>" + esc(c.name) + "</b><small>" + esc(c.sub) + '</small></span><button class="pm-call" data-act="call" data-id="' + c.id + '" aria-label="Call ' + esc(c.name) + '">' + icon("phone") + "</button></div>" +
        '<div class="pm-list">' + (rows || '<p class="muted pm-empty">Write to ' + esc(c.name.split(" ")[0]) + ".</p>") + (P.typing === c.id ? '<div class="pm-b pm-typing"><i></i><i></i><i></i></div>' : "") + "</div>" +
        '<form class="pm-send" data-form="msg"><input class="pm-in" type="text" maxlength="200" placeholder="Message" autocomplete="off"><button type="submit" class="pm-go" aria-label="Send">' + icon("chevron") + "</button></form></div>";
    }
    var ids = C.map(function (c) { return c.id; }).filter(function (id) { return (MSGS[id] || []).length; });
    var list = ids.map(function (id) { var c = contact(id), L = MSGS[id], last = L[L.length - 1], un = L.filter(function (m) { return !m.me && !m.read; }).length;
      return '<button class="pc-row pm-row ' + (un ? "new" : "") + '" data-act="msg-open" data-id="' + id + '">' + av(c) + "<span><b>" + esc(c.name) + "</b><small>" + (last.me ? "You: " : "") + esc(last.t) + "</small></span><em>" + hm(ago(last.at)) + (un ? "<i>" + un + "</i>" : "") + "</em></button>"; }).join("");
    var others = C.filter(function (c) { return !(MSGS[c.id] || []).length; }).map(function (c) { return '<button class="pc-row" data-act="msg-open" data-id="' + c.id + '">' + av(c) + "<span><b>" + esc(c.name) + "</b><small>" + esc(c.sub) + "</small></span></button>"; }).join("");
    return '<div class="pm-home"><div class="pa-h"><b>Messages</b></div>' + list + '<small class="pc-l">Write to</small>' + others + "</div>";
  }

  function callHtml() {
    if (P.calling) {
      var c = P.calling;
      return '<div class="pcall"><div class="pcall-who">' + av(c, true) + "<b>" + esc(c.name) + "</b><small>" + (P.callState === "ended" ? "Call ended" : P.callState === "talk" ? "00:0" + Math.min(9, P.callSecs || 0) : "Calling…") + "</small></div>" +
        '<div class="pcall-keys">' + [["Mute", "mute"], ["Keypad", "sliders"], ["Speaker", "volume"], ["Add call", "plus"], ["Video", "camera"], ["Contacts", "user"]].map(function (b) { return '<button class="pk-b" disabled><span>' + icon(b[1]) + "</span><small>" + b[0] + "</small></button>"; }).join("") + "</div>" +
        '<button class="pcall-end" data-act="call-end" aria-label="End call">' + icon("phone") + "</button></div>";
    }
    var tabs = [["fav", "Favourites", "star"], ["recents", "Recents", "clock"], ["contacts", "Contacts", "user"], ["keypad", "Keypad", "sliders"]], body = "";
    if (P.tab === "fav") body = '<div class="pa-h"><b>Favourites</b></div>' + C.filter(function (c) { return c.fam; }).map(function (c) { return '<button class="pc-row" data-act="call" data-id="' + c.id + '">' + av(c) + "<span><b>" + esc(c.name) + "</b><small>" + esc(c.sub) + '</small></span><em class="pc-ph">' + icon("phone") + "</em></button>"; }).join("");
    else if (P.tab === "recents") body = '<div class="pa-h"><b>Recents</b></div>' + CALLS.map(function (r) { var c = contact(r.id); return '<button class="pc-row" data-act="call" data-id="' + r.id + '">' + av(c) + "<span><b>" + esc(c.name) + "</b><small>" + (r.dir === "in" ? (r.ok ? "Incoming" : "Missed") : "Outgoing") + (r.ok ? " · " + Math.floor(r.dur / 60) + ":" + String(r.dur % 60).padStart(2, "0") : "") + "</small></span><em>" + hm(ago(r.at)) + "</em></button>"; }).join("");
    else if (P.tab === "contacts") { var letter = ""; body = '<div class="pa-h"><b>Contacts</b></div>' + C.map(function (c) { var l = c.fam ? "Family" : c.name.replace(/^Capt\.\s+/, "").charAt(0); var h = l !== letter ? '<small class="pc-l">' + esc(l) + "</small>" : ""; letter = l;
      return h + '<div class="pc-row pc-c">' + av(c) + "<span><b>" + esc(c.name) + "</b><small>" + esc(c.sub) + '</small></span><button class="pc-mini" data-act="msg-to" data-id="' + c.id + '" aria-label="Message">' + icon("chat") + '</button><button class="pc-mini" data-act="call" data-id="' + c.id + '" aria-label="Call">' + icon("phone") + "</button></div>"; }).join(""); }
    else {
      var keys = [["1", ""], ["2", "ABC"], ["3", "DEF"], ["4", "GHI"], ["5", "JKL"], ["6", "MNO"], ["7", "PQRS"], ["8", "TUV"], ["9", "WXYZ"], ["*", ""], ["0", "+"], ["#", ""]];
      body = '<div class="pkp"><div class="pkp-num">' + (esc(P.dial) || '<span class="muted">Enter a number</span>') + '</div><div class="pkp-grid">' + keys.map(function (k) { return '<button class="pkp-k" data-act="dial" data-k="' + k[0] + '"><b>' + k[0] + "</b><small>" + k[1] + "</small></button>"; }).join("") + "</div>" +
        '<div class="pkp-row"><span></span><button class="pkp-call" data-act="call" data-num="1" aria-label="Call">' + icon("phone") + "</button>" + (P.dial ? '<button class="pkp-del" data-act="dial" data-k="del" aria-label="Delete">' + icon("back") + "</button>" : "<span></span>") + "</div></div>";
    }
    return '<div class="pph"><div class="pph-b">' + body + '</div><nav class="pph-tabs">' + tabs.map(function (t) { return '<button class="' + (P.tab === t[0] ? "on" : "") + '" data-act="ph-tab" data-k="' + t[0] + '">' + icon(t[2]) + "<small>" + t[1] + "</small></button>"; }).join("") + "</nav></div>";
  }

  function storeHtml() {
    var row = function (a, sub, btn) { return '<div class="ps-row"><span class="ld-ic"><span style="--c:' + a[3] + '">' + icon(a[2]) + '</span></span><span class="ps-t"><b>' + esc(a[1]) + "</b><small>" + esc(sub) + "</small></span>" + btn + "</div>"; };
    var soon = [["", "Flight tracker", "map", "#4C8DF6", "Follow the aircraft at your gates on a map, live."], ["", "Weather", "wind", "#5BC0EB", "Forecasts and METARs for your airport."], ["", "Crew chat", "chat", "#34C759", "Group chats with your staff."]];
    return '<div class="pstore"><div class="pa-h"><b>Store</b></div><div class="ps-hero"><small>New</small><b>Messages and Phone</b><span>Write to and call your family and your staff. They answer what you say, and remember how you treat them.</span><button class="btn sm" data-act="app" data-k="msgs">Open</button></div>' +
      "<h4>Coming soon</h4>" + soon.map(function (a) { return row(a, a[4], '<button class="ps-get" disabled>Soon</button>'); }).join("") +
      "<h4>On your phone</h4>" + APPS.concat(PH_APPS.slice(0, 2)).map(function (a) { return row(a, "Installed", '<button class="ps-get" data-act="app" data-k="' + a[0] + '">Open</button>'); }).join("") + "</div>";
  }

  function marketsHtml() {
    var M = [["Jet fuel", "per 1,000 litres", 812, 2.4, "fuel"], ["Spare parts", "index", 1048, -0.8, "gear"], ["Food and drink", "per cabin load", 386, 1.1, "store"], ["Shop goods", "per pallet", 1290, 0.3, "market"]];
    return '<div class="adp-mk">' + M.map(function (m) { return '<div class="adp-mk-r"><span class="lw-ic" style="--c:#E98B3A">' + icon(m[4]) + "</span><span><b>" + m[0] + "</b><small>" + m[1] + '</small></span><span class="adp-mk-v"><b>' + money(m[2]) + '</b><small class="' + (m[3] > 0 ? "red" : "green") + '">' + (m[3] > 0 ? "▲ " : "▼ ") + Math.abs(m[3]).toFixed(1) + "%</small></span></div>"; }).join("") +
      '<p class="fine-print">' + icon("info") + "Prices move every day. Buy fuel and goods when they are low.</p></div>";
  }

  function photosHtml() {
    var imgs = PHOTOS;
    return imgs.length ? '<div class="adp-ph">' + imgs.map(function (u) { return '<img src="' + esc(u) + '" alt="" loading="lazy">'; }).join("") + "</div>" : '<div class="adp-info"><span>' + icon("camera") + "</span><b>Photos</b><p>The pictures you take in the game.</p></div>";
  }
  // #endregion

  // #region ── Taps ──
  var ACT = {
    app: function (el) { P.app = el.getAttribute("data-k") || ""; if (P.app !== "bank") { P.bankIn = false; } if (!P.app) { P.thread = ""; P.calling = null; P.mailId = ""; } },
    mail: function (el) { P.mailId = el.getAttribute("data-id"); MAIL.forEach(function (m) { if (m.id === P.mailId) m.read = true; }); },
    "mail-back": function () { P.mailId = ""; },
    "bank-login": function () { if (P.bankBusy) return; P.bankBusy = true; setTimeout(function () { P.bankBusy = false; P.bankIn = true; render(); }, 1400); },
    "bank-out": function () { P.bankIn = false; },
    "bank-tab": function (el) { P.bankTab = el.getAttribute("data-k"); },
    "bank-fil": function (el) { P.bankFil = el.getAttribute("data-k"); },
    "cal-day": function (el) { P.calDay = el.getAttribute("data-d"); },
    "msg-open": function (el) { P.thread = el.getAttribute("data-id"); },
    "msg-to": function (el) { P.app = "msgs"; P.thread = el.getAttribute("data-id"); },
    "msg-back": function () { P.thread = ""; },
    "ph-tab": function (el) { P.tab = el.getAttribute("data-k"); },
    dial: function (el) { var k = el.getAttribute("data-k"); P.dial = k === "del" ? P.dial.slice(0, -1) : (P.dial + k).slice(0, 14); },
    call: function (el) {
      var c = contact(el.getAttribute("data-id")) || { name: P.dial || "Unknown", c: "#8A94A6" }; P.app = "call"; P.calling = c; P.callState = "calling"; P.callSecs = 0;
      clearTimeout(P.ct); clearInterval(P.ci);
      P.ct = setTimeout(function () { if (P.calling !== c) return; P.callState = "talk"; render(); P.ci = setInterval(function () { P.callSecs++; if (P.calling === c && P.callState === "talk") render(); else clearInterval(P.ci); }, 1000); }, 2600);
    },
    "call-end": function () { clearTimeout(P.ct); clearInterval(P.ci); P.callState = "ended"; var c = P.calling; setTimeout(function () { if (P.calling === c) { P.calling = null; render(); } }, 900); }
  };
  var REPLY_FAM = ["Love you! See you tonight ❤️", "Okay, see you at home!", "Haha, thanks 😊", "Sounds good!"];
  var REPLY_STAFF = ["Understood, thanks.", "Will do.", "Got it, I'll take care of it.", "On it."];
  function send(text) {
    text = text.trim(); if (!text || !P.thread) return; var id = P.thread, c = contact(id);
    (MSGS[id] = MSGS[id] || []).push({ me: true, t: text, at: 0 });
    var R = c.fam ? REPLY_FAM : REPLY_STAFF, line = R[Math.floor(Math.random() * R.length)];
    setTimeout(function () { P.typing = id; render(); }, 900);
    setTimeout(function () { P.typing = ""; MSGS[id].push({ me: false, t: line, at: 0, read: P.thread === id && P.app === "msgs" }); render(); }, 2800);
  }
  // #endregion

  // #region ── Building the phone on the page and fitting it to its place ──
  function mount(box) {
    box.innerHTML = '<div class="adp"><div class="adp-scale"><div class="ph-frame"><div class="ph-glass"><div class="lap-os ph-os"><div class="lap-bar ph-status"></div><div class="lap-area"><div class="lap-desk"></div><div class="ph-dock"></div><div class="lap-win" hidden></div></div><button class="ph-home" data-act="app" data-k="" aria-label="Home"><i></i></button></div></div><span class="ph-glare"></span><span class="ph-cam"></span><span class="ph-side"></span></div></div></div>';
    try { PHOTOS = JSON.parse(box.getAttribute("data-photos") || "[]"); } catch (x) { PHOTOS = []; }
    root = box.querySelector(".ph-os"); frame = box.querySelector(".ph-frame");
    var sc = box.querySelector(".adp-scale"), fit = function () { var k = Math.min(1, box.clientWidth / 401); sc.style.transform = "scale(" + k + ")"; box.querySelector(".adp").style.height = Math.round(860 * k) + "px"; box.querySelector(".adp").style.width = Math.round(401 * k) + "px"; };
    fit(); window.addEventListener("resize", fit);
    box.addEventListener("click", function (e) {
      var el = e.target.closest("[data-act]"); if (!el || !box.contains(el) || el.disabled) return;
      var f = ACT[el.getAttribute("data-act")]; if (!f) return; e.preventDefault(); f(el); render();
    });
    box.addEventListener("submit", function (e) {
      e.preventDefault(); var f = e.target, i = f.querySelector("input"), v = i ? i.value : "";
      if (f.getAttribute("data-form") === "msg") send(v);
      if (f.getAttribute("data-form") === "cal" && v.trim()) { var k = P.calDay || ymd(now()); (NOTES[k] = NOTES[k] || []).push(v.trim()); }
      render(); var j = box.querySelector(".pm-in"); if (j) j.focus();
    });
    // the light on the glass follows the pointer, as in the game
    box.addEventListener("pointermove", function (e) { var r = frame.getBoundingClientRect(); frame.style.setProperty("--gx", ((e.clientX - r.left) / r.width - 0.5) * 40 + "%"); frame.style.setProperty("--gy", ((e.clientY - r.top) / r.height - 0.5) * 30 + "%"); });
    render(); setInterval(function () { if (!P.app || P.app === "calendar") render(); else root.querySelector(".ph-status").innerHTML = statusBar(); }, 30000);
  }
  function start() { Array.prototype.forEach.call(document.querySelectorAll("[data-game-phone]"), mount); }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", start); else start();
  // #endregion
})();
