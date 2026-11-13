/* Airport Dynasty Pro website: reveal on scroll, the feature slider, the hero pictures, the local clock, the two
   technology demos, the flight-plan plane, pictures, share and the launch-day sign-up. Own code, no libraries. */
(function () {
  "use strict";
  var doc = document, root = doc.documentElement, W = window, cfg = W.ADS || {};
  var reduce = W.matchMedia && W.matchMedia("(prefers-reduced-motion: reduce)").matches;
  root.classList.add("ad-js");
  var $$ = function (s, el) { return Array.prototype.slice.call((el || doc).querySelectorAll(s)); };
  function ready(fn) { if (doc.readyState !== "loading") fn(); else doc.addEventListener("DOMContentLoaded", fn); }

  ready(function () {
    // #region ── reveal on scroll, top bar, menu, scroll to top ──
    if ("IntersectionObserver" in W && !reduce) {
      var io = new IntersectionObserver(function (es) { es.forEach(function (e) { if (e.isIntersecting) { e.target.classList.add("is-in"); io.unobserve(e.target); } }); }, { rootMargin: "0px 0px -8% 0px", threshold: 0.06 });
      $$("[data-reveal]").forEach(function (el) { io.observe(el); });
    } else $$("[data-reveal]").forEach(function (el) { el.classList.add("is-in"); });
    var nav = doc.querySelector(".ad-nav"), top = doc.querySelector(".ad-top");
    var onScroll = function () { var y = W.scrollY || 0; if (nav) nav.classList.toggle("is-scrolled", y > 30); if (top) top.classList.toggle("is-on", y > W.innerHeight * 0.7); route(); };
    W.addEventListener("scroll", onScroll, { passive: true });
    var menu = nav && nav.querySelector(".ad-nav__menu");
    if (menu) menu.addEventListener("click", function () { var o = nav.classList.toggle("is-open"); menu.setAttribute("aria-expanded", o ? "true" : "false"); });
    $$(".ad-nav__links a").forEach(function (a) { a.addEventListener("click", function () { nav.classList.remove("is-open"); }); });
    if (top) top.addEventListener("click", function () { W.scrollTo({ top: 0, behavior: reduce ? "auto" : "smooth" }); });
    // #endregion

    // #region ── local time at the airport (Iceland keeps UTC all year) ──
    var clocks = $$("[data-clock]");
    var tick = function () { var d = new Date(), t = ("0" + d.getUTCHours()).slice(-2) + ":" + ("0" + d.getUTCMinutes()).slice(-2); clocks.forEach(function (c) { c.textContent = t; }); };
    if (clocks.length) { tick(); setInterval(tick, 10000); }
    // #endregion

    // #region ── hero pictures fade one into the next ──
    var bg = $$(".ad-hero__bg img"), cap = $$(".ad-hero__caption span"), hi = 0;
    if (bg.length > 1 && !reduce) setInterval(function () {
      bg[hi].classList.remove("is-on"); if (cap[hi]) cap[hi].classList.remove("is-on");
      hi = (hi + 1) % bg.length; bg[hi].classList.add("is-on"); if (cap[hi]) cap[hi].classList.add("is-on");
    }, 7000);
    // #endregion

    // #region ── the feature slider: arrows, dots, swipe or drag; moves on slowly by itself until the visitor takes over ──
    $$("[data-slider]").forEach(function (sl) {
      var win = sl.querySelector(".ad-feat__win"), track = sl.querySelector(".ad-feat__track"), slides = $$(".ad-feat__slide", sl), dots = $$(".ad-feat__dot", sl), now = sl.querySelector("[data-slider-now]");
      var n = slides.length, cur = 0, timer = 0, hold = false; if (!n) return;
      var go = function (i) {
        cur = (i + n) % n; track.style.transform = "translateX(" + (-cur * 100) + "%)";
        slides.forEach(function (s, k) { s.classList.toggle("is-on", k === cur); });
        dots.forEach(function (d, k) { d.classList.toggle("is-on", k === cur); });
        if (now) now.textContent = (cur < 9 ? "0" : "") + (cur + 1);
      };
      var stop = function () { clearInterval(timer); timer = 0; };
      sl.querySelector("[data-slider-prev]").addEventListener("click", function () { stop(); go(cur - 1); });
      sl.querySelector("[data-slider-next]").addEventListener("click", function () { stop(); go(cur + 1); });
      dots.forEach(function (d, k) { d.addEventListener("click", function () { stop(); go(k); }); });
      sl.addEventListener("keydown", function (e) { if (e.key === "ArrowLeft") { stop(); go(cur - 1); } if (e.key === "ArrowRight") { stop(); go(cur + 1); } });
      var x0 = 0, dx = 0, down = false;
      win.addEventListener("pointerdown", function (e) { down = true; x0 = e.clientX; dx = 0; win.classList.add("is-drag"); });
      win.addEventListener("pointermove", function (e) { if (!down) return; dx = e.clientX - x0; track.style.transform = "translateX(calc(" + (-cur * 100) + "% + " + dx + "px))"; });
      var end = function () { if (!down) return; down = false; win.classList.remove("is-drag"); if (Math.abs(dx) > 50) { stop(); go(cur + (dx < 0 ? 1 : -1)); } else go(cur); };
      win.addEventListener("pointerup", end); win.addEventListener("pointercancel", end); win.addEventListener("pointerleave", end);
      sl.addEventListener("mouseenter", function () { hold = true; }); sl.addEventListener("mouseleave", function () { hold = false; });
      go(0);
      if (!reduce && n > 1) timer = setInterval(function () { if (!hold) go(cur + 1); }, 9000);
    });
    // #endregion

    // #region ── numbers count up ──
    $$("[data-count]").forEach(function (el) {
      var m = /^(\D*)([\d,]+)(.*)$/.exec(el.textContent.trim()); if (!m || reduce) return;
      var end = +m[2].replace(/,/g, ""), comma = m[2].indexOf(",") >= 0, done = false;
      var fmt = function (n) { return comma ? n.toLocaleString("en-US") : String(n); };
      el.textContent = m[1] + "0" + m[3];
      var run = function () { if (done) return; done = true; var t0 = performance.now(); (function step(t) { var k = Math.min(1, (t - t0) / 1500), e = 1 - Math.pow(1 - k, 3); el.textContent = m[1] + fmt(Math.round(end * e)) + m[3]; if (k < 1) requestAnimationFrame(step); })(t0); };
      if ("IntersectionObserver" in W) { var o = new IntersectionObserver(function (es) { if (es[0].isIntersecting) { run(); o.disconnect(); } }); o.observe(el); } else run();
    });
    // #endregion

    // #region ── pictures: arrows, drag, larger view ──
    $$(".ad-gallery").forEach(function (g) {
      var tr = g.querySelector(".ad-gallery__track"); if (!tr) return;
      $$(".ad-round", g).forEach(function (b) { b.addEventListener("click", function () { tr.scrollBy({ left: +b.dataset.dir * tr.clientWidth * 0.6, behavior: "smooth" }); }); });
      var down = false, sx = 0, sl = 0, moved = 0;
      tr.addEventListener("pointerdown", function (e) { if (e.pointerType !== "mouse") return; down = true; moved = 0; sx = e.clientX; sl = tr.scrollLeft; tr.classList.add("is-drag"); });
      W.addEventListener("pointermove", function (e) { if (!down) return; moved = Math.abs(e.clientX - sx); tr.scrollLeft = sl - (e.clientX - sx); });
      W.addEventListener("pointerup", function () { if (!down) return; down = false; tr.classList.remove("is-drag"); });
      $$(".ad-shot__btn", g).forEach(function (b) { b.addEventListener("click", function (e) { if (moved > 6) { e.preventDefault(); return; } var im = b.querySelector("img"); openShot(im.currentSrc || im.src, im.alt); }); });
    });
    function openShot(src, alt) {
      var lb = doc.createElement("div"); lb.className = "ad-lb"; lb.setAttribute("role", "dialog"); lb.setAttribute("aria-label", alt || "Picture");
      var im = new Image(); im.src = src; im.alt = alt || ""; lb.appendChild(im);
      if (alt) { var p = doc.createElement("p"); p.textContent = alt; lb.appendChild(p); }
      doc.body.appendChild(lb); requestAnimationFrame(function () { lb.classList.add("is-open"); });
      var close = function () { lb.classList.remove("is-open"); setTimeout(function () { lb.remove(); }, 350); doc.removeEventListener("keydown", key); };
      var key = function (e) { if (e.key === "Escape") close(); };
      lb.addEventListener("click", close); doc.addEventListener("keydown", key);
    }
    // #endregion

    // #region ── trailer: loads the player only when Play is pressed ──
    $$(".ad-film__screen[data-src]").forEach(function (p) {
      var b = p.querySelector(".ad-film__play"); if (!b) return;
      b.addEventListener("click", function () {
        var f = doc.createElement("iframe"); f.src = p.getAttribute("data-src"); f.allow = "autoplay; fullscreen; picture-in-picture"; f.allowFullscreen = true; f.title = "Trailer";
        p.appendChild(f); b.remove(); var n = p.querySelector(".ad-film__note"); if (n) n.remove();
      });
    });
    // #endregion

    // #region ── Lumen demo: the slider moves the sun ──
    $$("[data-lumen]").forEach(function (d) {
      var r = d.querySelector("[data-lumen-range]"); if (!r) return;
      var set = function () { d.style.setProperty("--t", (r.value / 100).toFixed(3)); };
      r.addEventListener("input", set); set();
      if (!reduce) { var auto = true, v = +r.value; r.addEventListener("pointerdown", function () { auto = false; });
        setInterval(function () { if (!auto) return; v = (v + 0.4) % 100; r.value = v; set(); }, 60); }
    });
    // #endregion

    // #region ── Nanite demo: a hill of triangles, denser the closer you are ──
    $$("[data-nanite]").forEach(function (cv) {
      var r = cv.parentNode.querySelector("[data-nanite-range]"), ctx = cv.getContext("2d"); if (!ctx) return;
      var draw = function () {
        var dpr = Math.min(2, W.devicePixelRatio || 1), w = cv.clientWidth, h = cv.clientHeight; cv.width = w * dpr; cv.height = h * dpr; ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        ctx.clearRect(0, 0, w, h);
        var near = 1 - (r ? r.value / 100 : 0.35), n = Math.round(6 + near * 46), cx = w / 2, base = h * 0.82, wid = w * (0.45 + near * 0.5), ht = h * (0.35 + near * 0.35);
        var pt = function (i, j) { var u = i / n, v = j / n, x = cx + (u - 0.5) * wid, bump = Math.sin(u * Math.PI) * Math.sin(v * Math.PI * 0.5 + 0.3), y = base - ht * bump * (1 - v * 0.35) - v * h * 0.18; return [x + (v - 0.5) * 30, y]; };
        ctx.lineWidth = 0.8; var tris = 0;
        for (var j = 0; j < n; j++) for (var i = 0; i < n; i++) {
          var a = pt(i, j), b = pt(i + 1, j), c = pt(i, j + 1), d = pt(i + 1, j + 1);
          [[a, b, c], [b, d, c]].forEach(function (t, k) {
            var shade = 0.25 + 0.5 * ((i + j + k) % 7) / 7;
            ctx.beginPath(); ctx.moveTo(t[0][0], t[0][1]); ctx.lineTo(t[1][0], t[1][1]); ctx.lineTo(t[2][0], t[2][1]); ctx.closePath();
            ctx.fillStyle = "hsla(" + (190 + (i * 7 + j * 11) % 60) + ",80%," + (30 + shade * 30) + "%,.55)"; ctx.fill();
            ctx.strokeStyle = "rgba(255,204,0,.35)"; ctx.stroke(); tris++;
          });
        }
        ctx.fillStyle = "rgba(238,243,251,.85)"; ctx.font = "700 14px 'AD Barlow', sans-serif"; ctx.fillText(tris.toLocaleString("en-US") + " TRIANGLES DRAWN", 14, h - 58);
      };
      if (r) r.addEventListener("input", draw); W.addEventListener("resize", draw); draw();
    });
    // #endregion

    // #region ── four ways to play: tabs switch by click, and by themselves every 7 seconds until clicked ──
    $$("[data-tabs]").forEach(function (panel) {
      var tabs = $$(".ad-play__tab", panel), views = $$(".ad-play__view", panel), cur = 0, timer = 0;
      var show = function (i) {
        cur = i; tabs.forEach(function (t, k) { t.classList.toggle("is-on", k === i); t.setAttribute("aria-selected", k === i ? "true" : "false"); });
        views.forEach(function (v, k) { v.classList.toggle("is-on", k === i); });
        tabs[i].querySelector(".ad-play__bar").style.animation = "none"; void tabs[i].offsetWidth; tabs[i].querySelector(".ad-play__bar").style.animation = "";
      };
      tabs.forEach(function (t, k) { t.addEventListener("click", function () { clearInterval(timer); panel.classList.add("is-paused"); show(k); }); });
      if (!reduce && tabs.length > 1) timer = setInterval(function () { show((cur + 1) % tabs.length); }, 7000);
    });
    // #endregion

    // #region ── share ──
    $$("[data-share]").forEach(function (a) {
      a.addEventListener("click", function (e) { e.preventDefault(); var u = encodeURIComponent(location.href.split("#")[0]), t = encodeURIComponent(doc.title);
        W.open(a.getAttribute("data-share").replace("{u}", u).replace("{t}", t), "_blank", "noopener,width=640,height=560"); });
    });
    $$(".ad-share__copy").forEach(function (b) {
      b.addEventListener("click", function () { var u = location.href.split("#")[0];
        (navigator.clipboard ? navigator.clipboard.writeText(u) : Promise.reject()).then(function () { b.classList.add("is-done"); b.title = cfg.copied || "Link copied"; setTimeout(function () { b.classList.remove("is-done"); }, 2000); }).catch(function () {}); });
    });
    // #endregion

    // #region ── launch-day sign-up ──
    var f = doc.querySelector(".ad-notify");
    if (f) f.addEventListener("submit", function (e) {
      e.preventDefault();
      var i = f.querySelector("input[type=email]"), m = f.querySelector(".ad-notify__msg"), b = f.querySelector("button");
      if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(i.value)) { m.textContent = cfg.bad || "Please check the email address."; m.classList.remove("is-ok"); return; }
      if (!cfg.ajax) { m.textContent = cfg.preview || "Thank you!"; m.classList.add("is-ok"); return; }
      b.disabled = true;
      var d = new FormData(); d.append("action", "ads_notify"); d.append("nonce", cfg.nonce); d.append("email", i.value);
      var hp = f.querySelector("input[name=website]"); if (hp) d.append("website", hp.value);
      fetch(cfg.ajax, { method: "POST", body: d, credentials: "same-origin" }).then(function (r) { return r.json(); }).then(function (r) {
        var ok = !!(r && r.success); m.textContent = ok ? cfg.thanks : cfg.fail; m.classList.toggle("is-ok", ok); if (ok) i.value = "";
      }).catch(function () { m.textContent = cfg.fail; }).then(function () { b.disabled = false; });
    });
    // #endregion

    // #region ── flight plan: the plane follows the dashed route as you scroll ──
    var map = doc.querySelector("[data-route]"), path = map && map.querySelector(".ad-route__line path"), plane = map && map.querySelector(".ad-route__plane");
    function route() {
      if (!path || !plane || !path.getTotalLength || plane.offsetParent === null) return;
      var r = map.getBoundingClientRect(), vh = W.innerHeight, k = Math.min(1, Math.max(0, (vh - r.top) / (vh + r.height * 0.4)));
      var L = path.getTotalLength(), p = path.getPointAtLength(k * L), p2 = path.getPointAtLength(Math.min(L, k * L + 4));
      var svg = path.ownerSVGElement, sx = svg.clientWidth / 1000, sy = svg.clientHeight / 120;
      plane.style.left = (p.x * sx) + "px"; plane.style.top = (p.y * sy) + "px";
      plane.style.transform = "rotate(" + (Math.atan2((p2.y - p.y) * sy, (p2.x - p.x) * sx) * 180 / Math.PI + 90) + "deg)";
    }
    W.addEventListener("resize", route); onScroll();
    // #endregion
  });
})();
