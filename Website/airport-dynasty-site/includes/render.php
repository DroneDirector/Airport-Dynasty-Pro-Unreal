<?php
/**
 * The page's sections as plain functions: each takes its settings (from Elementor, or the defaults in content.php)
 * and prints its HTML. The Elementor widgets call these; so does tools/preview.php for a quick look outside WordPress.
 */
if (!defined('ABSPATH')) exit;

// #region ── Helpers ──

function ads_rich($s) { return wp_kses_post((string) $s); }
function ads_lines($s) { return array_values(array_filter(array_map('trim', explode("\n", (string) $s)), 'strlen')); }
function ads_img_url($img) { return is_array($img) ? (string) ($img['url'] ?? '') : (string) $img; }

/** Pictograms in the style of airport wayfinding signs (own drawings). */
function ads_pict($k) {
    $p = [
        'terminal' => '<path d="M3 20h18M5 20V9l7-4 7 4v11"/><path d="M9 20v-5h6v5M8 11h2M14 11h2"/>',
        'tower'    => '<path d="M9 21h6M10 21l.7-9h2.6l.7 9"/><path d="M6.5 8h11l-1.8 4H8.3z"/><path d="M9 8V5.5h6V8M12 5.5V3"/>',
        'hangar'   => '<path d="M2 20h20M3.5 20v-7a8.5 8.5 0 0 1 17 0v7"/><path d="M8 20v-5h8v5"/>',
        'fire'     => '<path d="M12 3c1 3.5 5 5 5 10a5 5 0 0 1-10 0c0-2.5 1.5-4 2.5-5 .2 1.8 1 2.8 2 3 0-3 .5-5.5.5-8z"/>',
        'car'      => '<path d="M4 16v-3l2-5h12l2 5v3"/><path d="M3 16h18v2H3z"/><circle cx="7.5" cy="18.5" r="1.5"/><circle cx="16.5" cy="18.5" r="1.5"/><path d="M6.5 12h11"/>',
        'home'     => '<path d="M3 11l9-7 9 7"/><path d="M5 10v10h14V10"/><path d="M10 20v-5h4v5"/>',
        'more'     => '<circle cx="6" cy="12" r="1.6"/><circle cx="12" cy="12" r="1.6"/><circle cx="18" cy="12" r="1.6"/>',
        'plane'    => '<path d="M21 15.5v-2L13.5 9V4.5a1.5 1.5 0 0 0-3 0V9L3 13.5v2l7.5-2.2V18l-2 1.5V21l3.5-1 3.5 1v-1.5l-2-1.5v-4.7z"/>',
        'play'     => '<path d="M8 5.5v13l11-6.5z"/>',
        'cart'     => '<path d="M3 4h2.5l2.2 11h10.6L20.5 7H7"/><circle cx="9.5" cy="19" r="1.5"/><circle cx="17" cy="19" r="1.5"/>',
        'bell'     => '<path d="M6 16V11a6 6 0 0 1 12 0v5l2 2H4z"/><path d="M10 20a2 2 0 0 0 4 0"/>',
        'arrow'    => '<path d="M5 12h14M13 6l6 6-6 6"/>',
        'menu'     => '<path d="M4 7h16M4 12h16M4 17h16"/>',
        'sun'      => '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',
        'mesh'     => '<path d="M3 18l5-11 5 7 3-4 5 8z"/><path d="M8 7l5 11M13 14l3 4M16 10l-3 4"/>',
    ][$k] ?? '';
    return '<svg class="ad-pict" viewBox="0 0 24 24" aria-hidden="true" focusable="false">' . $p . '</svg>';
}

function ads_logo() {
    return '<span class="ad-logo__mark">' . ads_pict('tower') . '</span><span class="ad-logo__name">Airport Dynasty <b>Pro</b></span>';
}

/** Where buying leads: the WooCommerce product (straight to checkout), another shop, or nothing yet. */
function ads_buy_target() {
    $pid = (int) ads_opt('product_id');
    if ($pid && function_exists('wc_get_product')) {
        $p = wc_get_product($pid);
        if ($p && $p->is_purchasable() && $p->is_in_stock()) {
            return ['url' => add_query_arg('add-to-cart', $pid, wc_get_checkout_url()), 'price' => wp_strip_all_tags(wc_price(wc_get_price_to_display($p)))];
        }
    }
    if (ads_opt('buy_url')) return ['url' => ads_opt('buy_url'), 'price' => ''];
    return null;
}

/** "Buy now · price" once the game is on sale, otherwise "Coming soon" leading to the email list. */
function ads_buy_button($cls = '') {
    $t = ads_buy_target();
    if ($t) return '<a class="ad-btn ad-btn--buy ' . esc_attr($cls) . '" href="' . esc_url($t['url']) . '">' . ads_pict('cart') . '<span>Buy now' . ($t['price'] ? '<small>' . esc_html($t['price']) . ' · PC</small>' : '<small>For PC</small>') . '</span></a>';
    return '<a class="ad-btn ad-btn--buy is-soon ' . esc_attr($cls) . '" href="#buy">' . ads_pict('bell') . '<span>Coming soon<small>Get notified</small></span></a>';
}

/** Inline SVG for a social / share icon (Simple Icons, CC0) or one of our own. */
function ads_icon($name) {
    static $icons = null;
    if ($icons === null) $icons = json_decode((string) file_get_contents(ADS_DIR . 'data/icons.json'), true) ?: [];
    $own = [
        'mail' => 'M2 5h20v14H2zm2 2v.3l8 5.2 8-5.2V7zm16 2.7-8 5.2-8-5.2V17h16z',
        'link' => 'M10.6 13.4a1 1 0 0 1 0-1.4l3.5-3.5a1 1 0 1 1 1.4 1.4L12 13.4a1 1 0 0 1-1.4 0zM8 21a5 5 0 0 1-3.5-8.5l2.1-2.1a1 1 0 1 1 1.4 1.4l-2.1 2.1a3 3 0 0 0 4.2 4.2l2.1-2.1a1 1 0 1 1 1.4 1.4l-2.1 2.1A5 5 0 0 1 8 21zm9.3-7.2a1 1 0 0 1-.7-1.7l2.1-2.1a3 3 0 0 0-4.2-4.2l-2.1 2.1a1 1 0 1 1-1.4-1.4l2.1-2.1a5 5 0 0 1 7 7l-2.1 2.1a1 1 0 0 1-.7.3z',
    ];
    $d = $own[$name] ?? ($icons[$name] ?? '');
    return '<svg class="ad-ico" viewBox="0 0 24 24" aria-hidden="true" focusable="false"><path d="' . esc_attr($d) . '"/></svg>';
}

/** A list whose "…and much more" line is styled as the closing line. */
function ads_points($s, $cls = 'ad-points') {
    $o = '<ul class="' . esc_attr($cls) . '">';
    foreach (ads_lines($s) as $l) {
        $more = stripos($l, 'much more') !== false;
        $o .= '<li' . ($more ? ' class="is-more"' : '') . '>' . esc_html($l) . '</li>';
    }
    return $o . '</ul>';
}

function ads_head($s, $center = true) {
    return '<div class="ad-head' . ($center ? ' ad-head--c' : '') . '"><p class="ad-tag" data-reveal><span></span>' . esc_html($s['eyebrow']) . '</p><h2 class="ad-h2" data-reveal>' . ads_rich($s['title']) . '</h2>'
        . (!empty($s['text']) ? '<p class="ad-lead" data-reveal>' . esc_html($s['text']) . '</p>' : '') . '</div>';
}

// #endregion

// #region ── Top bar ──

function ads_r_nav($s) {
    $home = ads_page_url('home');
    $links = [['Buildings', '#buildings'], ['People', '#people'], ['Play', '#play'], ['Technology', '#tech'], ['Flight plan', '#route']];
    echo '<header class="ad ad-nav"><div class="ad-wrap ad-nav__in"><a class="ad-logo" href="' . esc_url($home) . '">' . ads_logo() . '</a><nav class="ad-nav__links" aria-label="Main">';
    foreach ($links as $l) echo '<a href="' . esc_url($l[1]) . '">' . esc_html($l[0]) . '</a>';
    echo '</nav><span class="ad-nav__clock" title="Local time at Mýrdalssandur"><i></i>MÝRDALSSANDUR <b data-clock>--:--</b></span>' . ads_buy_button('ad-btn--sm');
    echo '<button class="ad-nav__menu" type="button" aria-label="Menu" aria-expanded="false">' . ads_pict('menu') . '</button></div><div class="ad-runway" aria-hidden="true"></div></header>';
}

// #endregion

// #region ── Hero: crossfading pictures and the departures board ──

function ads_r_hero($s) {
    $imgs = array_values(array_filter((array) ($s['images'] ?? []), function ($i) { return ads_img_url($i['image'] ?? '') !== ''; }));
    echo '<section class="ad ad-hero" id="top"><div class="ad-hero__bg" aria-hidden="true">';
    foreach ($imgs as $i => $it) echo '<img class="' . ($i === 0 ? 'is-on' : '') . '" src="' . esc_url(ads_img_url($it['image'])) . '" alt="" decoding="async"' . ($i ? ' loading="lazy"' : '') . '>';
    echo '</div><div class="ad-hero__shade" aria-hidden="true"></div><div class="ad-wrap ad-hero__in"><div class="ad-hero__copy">';
    echo '<p class="ad-sign" data-reveal>' . ads_pict('plane') . '<span>' . esc_html($s['sign']) . '</span><b>→</b></p>';
    echo '<h1 class="ad-h1" data-reveal>' . ads_rich($s['title']) . '</h1><p class="ad-lead" data-reveal>' . esc_html($s['text']) . '</p>';
    echo '<div class="ad-hero__cta" data-reveal>' . ads_buy_button() . '<a class="ad-btn ad-btn--ghost" href="#film">' . ads_pict('play') . '<span>' . esc_html($s['film']) . '</span></a></div>';
    echo '<p class="ad-hero__note" data-reveal>' . esc_html($s['note']) . '</p></div>';
    $rows = array_map(function ($l) { return array_pad(array_map('trim', explode('|', $l)), 4, ''); }, ads_lines($s['board']));
    echo '<div class="ad-board" data-reveal data-board="' . esc_attr(wp_json_encode($rows)) . '"><div class="ad-board__top"><span>' . ads_pict('plane') . ' Departures</span><span data-clock>--:--</span></div>';
    echo '<div class="ad-board__head"><span>Flight</span><span>To</span><span>Gate</span><span>Status</span></div><div class="ad-board__rows">';
    foreach (array_slice($rows, 0, 5) as $r) echo '<div class="ad-board__row"><span>' . esc_html($r[0]) . '</span><span>' . esc_html($r[1]) . '</span><span>' . esc_html($r[2]) . '</span><span class="st">' . esc_html($r[3]) . '</span></div>';
    echo '</div></div></div><div class="ad-hero__caption" aria-hidden="true">';
    foreach ($imgs as $i => $it) echo '<span class="' . ($i === 0 ? 'is-on' : '') . '">' . esc_html($it['caption'] ?? '') . '</span>';
    echo '</div></section>';
}

function ads_r_ticker($s) {
    $t = esc_html($s['ticker']);
    echo '<div class="ad ad-ticker" aria-label="' . $t . '"><div class="ad-ticker__track" aria-hidden="true"><span>' . $t . '</span><span>' . $t . '</span></div></div>';
}

function ads_r_stats($s) {
    echo '<section class="ad ad-stats"><div class="ad-wrap ad-stats__in">';
    foreach ((array) $s['items'] as $i => $it) echo '<div class="ad-stat" data-reveal style="--d:' . (int) $i . '"><b data-count>' . esc_html($it['num']) . '</b><span>' . esc_html($it['label']) . '</span><i class="ad-lights" aria-hidden="true"></i></div>';
    echo '</div></section>';
}

// #endregion

// #region ── Trailer ──

/** Player address from a YouTube or Vimeo link (privacy modes on); loads only after Play is pressed. */
function ads_player($url) {
    $url = (string) $url;
    if (preg_match('~(?:youtube\.com/(?:watch\?v=|embed/|shorts/)|youtu\.be/)([\w-]{11})~', $url, $m)) return 'https://www.youtube-nocookie.com/embed/' . $m[1] . '?autoplay=1&rel=0&modestbranding=1';
    if (preg_match('~vimeo\.com/(?:video/)?(\d+)(?:/([0-9a-f]+))?~i', $url, $m)) return 'https://player.vimeo.com/video/' . $m[1] . '?dnt=1&title=0&byline=0&portrait=0&autoplay=1' . (!empty($m[2]) ? '&h=' . $m[2] : '');
    return '';
}

function ads_r_video($s) {
    $src = ads_player($s['link']);
    echo '<section class="ad ad-film" id="film"><div class="ad-wrap">' . ads_head($s);
    echo '<div class="ad-film__screen" data-reveal' . ($src ? ' data-src="' . esc_url($src) . '"' : '') . '><span class="ad-film__corner"></span>';
    if ($src) echo '<button class="ad-film__play" type="button" aria-label="Play the trailer">' . ads_pict('play') . '</button><span class="ad-film__note">Press play to load the video</span>';
    else echo '<span class="ad-film__play is-idle">' . ads_pict('play') . '</span><span class="ad-film__note">' . esc_html($s['soon']) . '</span>';
    echo '</div></div></section>';
}

// #endregion

// #region ── Buildings: wayfinding signs ──

function ads_r_buildings($s) {
    echo '<section class="ad ad-buildings" id="buildings"><div class="ad-wrap">' . ads_head($s, false) . '<div class="ad-signs">';
    foreach ((array) $s['items'] as $i => $it) {
        $st = strtolower((string) $it['status']);
        $cls = strpos($st, 'game') !== false ? 'is-live' : (strpos($st, 'built') !== false ? 'is-wip' : 'is-soon');
        echo '<div class="ad-signboard ' . $cls . '" data-reveal style="--d:' . (int) ($i % 4) . '"><span class="ad-signboard__pict">' . ads_pict($it['icon']) . '</span>'
            . '<div class="ad-signboard__txt"><h3>' . esc_html($it['name']) . '</h3><p>' . esc_html($it['text']) . '</p></div>'
            . '<span class="ad-signboard__arrow" aria-hidden="true">' . esc_html($it['arrow']) . '</span><span class="ad-signboard__status">' . esc_html($it['status']) . '</span></div>';
    }
    echo '</div></div></section>';
}

// #endregion

// #region ── Holographic map: the people ──

function ads_r_holo($s) {
    echo '<section class="ad ad-holo" id="people"><div class="ad-wrap ad-holo__in"><div class="ad-holo__copy">' . ads_head($s, false) . '<div data-reveal>' . ads_points($s['points']) . '</div></div>';
    // The map: a terminal floor plan drawn like the game's holographic map, with people walking and an incident blinking.
    $paths = ['M60 250 H300 V120 H520', 'M80 270 H380 V200 H600', 'M520 120 V70 H700', 'M600 200 H700 V300', 'M300 120 V70 H120', 'M380 200 V300 H560'];
    echo '<div class="ad-holo__map" data-reveal aria-hidden="true"><svg viewBox="0 0 760 360"><defs><pattern id="adgrid" width="20" height="20" patternUnits="userSpaceOnUse"><path d="M20 0H0V20" fill="none" stroke="rgba(90,216,255,.12)" stroke-width="1"/></pattern></defs>';
    echo '<rect width="760" height="360" fill="url(#adgrid)"/><g class="ad-holo__plan"><path d="M40 40 H720 V320 H40 Z"/><path d="M40 160 H260 M260 40 V320 M460 40 V200 M460 200 H720 M560 200 V320 M140 220 h80 v60 h-80z M300 230 h120 v50 h-120z"/>';
    foreach ([80, 200, 330, 460, 590] as $x) echo '<path class="ad-holo__gate" d="M' . ($x + 20) . ' 40 v-18 h40 v18"/>';
    echo '</g><g class="ad-holo__planes">';
    foreach ([100, 350, 610] as $x) echo '<path d="M' . $x . ' 18 l14 -10 h6 l-6 10 h18 l6 -6 h4 l-3 6 l3 6 h-4 l-6 -6 h-18 l6 10 h-6z"/>';
    echo '</g><g class="ad-holo__people">';
    foreach ($paths as $i => $d) {
        for ($k = 0; $k < 3; $k++) echo '<circle r="3.2"><animateMotion dur="' . (9 + $i * 2) . 's" begin="-' . ($k * (3 + $i)) . 's" repeatCount="indefinite" path="' . $d . '"/></circle>';
    }
    echo '</g><g class="ad-holo__incident"><circle cx="520" cy="250" r="7"/><circle class="ring" cx="520" cy="250" r="7"/></g>';
    echo '<text x="56" y="340" class="ad-holo__label">TERMINAL · FLOOR 1</text><text x="704" y="340" text-anchor="end" class="ad-holo__label ad-holo__label--r">INCIDENT · GATE C</text></svg>';
    echo '<span class="ad-holo__c ad-holo__c--tl"></span><span class="ad-holo__c ad-holo__c--tr"></span><span class="ad-holo__c ad-holo__c--bl"></span><span class="ad-holo__c ad-holo__c--br"></span></div>';
    echo '</div></section>';
}

// #endregion

// #region ── Four ways to play: a panel with four tabs, each with its picture and list ──

function ads_r_passes($s) {
    $items = array_values((array) $s['items']);
    echo '<section class="ad ad-play" id="play"><div class="ad-wrap">' . ads_head($s) . '<div class="ad-play__panel" data-reveal data-tabs>';
    echo '<div class="ad-play__tabs" role="tablist">';
    foreach ($items as $i => $it) {
        echo '<button type="button" role="tab" class="ad-play__tab' . ($i === 0 ? ' is-on' : '') . '" aria-selected="' . ($i === 0 ? 'true' : 'false') . '" data-tab="' . (int) $i . '">'
            . '<span class="ad-play__n">' . sprintf('%02d', $i + 1) . '</span><span class="ad-play__name">' . esc_html($it['name']) . '</span><i class="ad-play__bar"></i></button>';
    }
    echo '</div><div class="ad-play__views">';
    foreach ($items as $i => $it) {
        echo '<div class="ad-play__view' . ($i === 0 ? ' is-on' : '') . '" role="tabpanel" data-view="' . (int) $i . '"><div class="ad-play__img"><img src="' . esc_url(ads_img_url($it['image'])) . '" alt=""' . ($i === 0 ? '' : ' loading="lazy"') . ' decoding="async"></div>'
            . '<div class="ad-play__copy"><h3>' . esc_html($it['name']) . '</h3>' . ads_points($it['points']) . '</div></div>';
    }
    echo '</div></div></div></section>';
}

// #endregion

// #region ── Pictures ──

function ads_r_gallery($s) {
    echo '<section class="ad ad-gallery" id="gallery"><div class="ad-wrap"><div class="ad-gallery__head">' . ads_head($s, false);
    echo '<div class="ad-gallery__nav"><button type="button" class="ad-round" data-dir="-1" aria-label="Previous">' . ads_pict('arrow') . '</button><button type="button" class="ad-round" data-dir="1" aria-label="Next">' . ads_pict('arrow') . '</button></div></div></div>';
    echo '<div class="ad-gallery__track" tabindex="0">';
    foreach ((array) $s['shots'] as $i => $it) {
        if (!ads_img_url($it['image'])) continue;
        echo '<figure class="ad-shot" data-reveal style="--d:' . (int) ($i % 4) . '"><button type="button" class="ad-shot__btn" aria-label="Show larger: ' . esc_attr($it['caption']) . '"><img src="' . esc_url(ads_img_url($it['image'])) . '" alt="' . esc_attr($it['caption']) . '" loading="lazy" decoding="async"></button><figcaption><span>' . sprintf('%02d', $i + 1) . '</span>' . esc_html($it['caption']) . '</figcaption></figure>';
    }
    echo '</div>' . (!empty($s['note']) ? '<p class="ad-wrap ad-note">' . esc_html($s['note']) . '</p>' : '') . '</section>';
}

// #endregion

// #region ── Technology: Nanite and Lumen, look and performance, with two small demos ──

function ads_r_tech($s) {
    echo '<section class="ad ad-tech" id="tech"><div class="ad-wrap">' . ads_head($s) . '<div class="ad-tech__grid">';
    echo '<article class="ad-tech__card" data-reveal><div class="ad-tech__demo ad-tech__demo--nanite"><canvas data-nanite aria-hidden="true"></canvas><label class="ad-slider"><span>Distance</span><input type="range" min="0" max="100" value="35" data-nanite-range aria-label="Camera distance"></label><p class="ad-tech__hint">Closer = more triangles drawn. Further = fewer. Always just enough.</p></div>';
    echo '<h3>' . ads_pict('mesh') . ' Nanite</h3><dl class="ad-tech__dl"><dt>The look</dt><dd>' . esc_html($s['nanite_look']) . '</dd><dt>Performance</dt><dd>' . esc_html($s['nanite_perf']) . '</dd></dl></article>';
    echo '<article class="ad-tech__card" data-reveal style="--d:1"><div class="ad-tech__demo ad-tech__demo--lumen" data-lumen style="--t:.35"><div class="ad-lumen__sky"></div><div class="ad-lumen__sun"></div>'
        . '<svg viewBox="0 0 400 160" aria-hidden="true"><path class="ad-lumen__ground" d="M0 130 H400 V160 H0z"/><path class="ad-lumen__shadow" d="M70 130 L330 130 L360 150 L100 150z"/><path class="ad-lumen__bld" d="M70 130 V80 L200 55 L330 80 V130z"/><path class="ad-lumen__glass" d="M95 125 V90 H305 V125z"/><path class="ad-lumen__glow" d="M95 125 V90 H305 V125z"/></svg>'
        . '<label class="ad-slider"><span>Time of day</span><input type="range" min="0" max="100" value="35" data-lumen-range aria-label="Time of day"></label><p class="ad-tech__hint">Move the sun: the light, the glass and the shadows follow.</p></div>';
    echo '<h3>' . ads_pict('sun') . ' Lumen</h3><dl class="ad-tech__dl"><dt>The look</dt><dd>' . esc_html($s['lumen_look']) . '</dd><dt>Performance</dt><dd>' . esc_html($s['lumen_perf']) . '</dd></dl></article>';
    echo '</div><p class="ad-note" data-reveal>' . esc_html($s['note']) . '</p></div></section>';
}

// #endregion

// #region ── Flight plan (roadmap) ──

function ads_r_route($s) {
    $n = count((array) $s['items']);
    echo '<section class="ad ad-route" id="route"><div class="ad-wrap">' . ads_head($s) . '<div class="ad-route__map" data-route><svg class="ad-route__line" viewBox="0 0 1000 120" preserveAspectRatio="none" aria-hidden="true"><path d="M0 90 C 200 10, 300 110, 500 60 S 800 10, 1000 50"/></svg>'
        . '<span class="ad-route__plane" aria-hidden="true">' . ads_pict('plane') . '</span><ol class="ad-route__stops" style="--n:' . (int) max(1, $n) . '">';
    foreach ((array) $s['items'] as $i => $it) echo '<li data-reveal style="--d:' . (int) $i . '"><span class="ad-route__when">' . esc_html($it['when']) . '</span><h3>' . esc_html($it['title']) . '</h3><p>' . esc_html($it['text']) . '</p></li>';
    echo '</ol></div></div></section>';
}

// #endregion

// #region ── Buy: the boarding pass, share, footer ──

function ads_r_buy($s) {
    $t = ads_buy_target();
    echo '<section class="ad ad-buy" id="buy"><div class="ad-wrap"><div class="ad-launch" data-reveal><div class="ad-launch__lights" aria-hidden="true"><i></i><i></i><i></i><i></i><i></i><i></i><i></i></div>';
    echo '<div class="ad-launch__copy"><p class="ad-sign">' . ads_pict('bell') . '<span>' . ($t ? 'Out now' : 'Launch day') . '</span></p><h2 class="ad-h2">' . esc_html($s['title']) . '</h2><p class="ad-lead">' . esc_html($t ? $s['live'] : $s['text']) . '</p></div>';
    echo '<div class="ad-launch__act">';
    if ($t) echo ads_buy_button();
    else echo '<form class="ad-notify" novalidate><input type="email" required placeholder="you@example.com" aria-label="Your email" autocomplete="email"><input type="text" name="website" tabindex="-1" autocomplete="off" class="ad-hp" aria-hidden="true"><button class="ad-btn ad-btn--buy" type="submit">' . ads_pict('bell') . '<span>' . esc_html($s['button']) . '</span></button><span class="ad-notify__msg" aria-live="polite">' . esc_html($s['small']) . '</span></form>';
    echo '<ul class="ad-launch__facts"><li><small>Platform</small><b>PC</b></li><li><small>Engine</small><b>Unreal Engine 5</b></li><li><small>Sold</small><b>Right here</b></li></ul></div></div></div></section>';
}

function ads_r_share($s) {
    $nets = ['facebook' => ['Facebook', 'https://www.facebook.com/sharer/sharer.php?u={u}'], 'x' => ['X', 'https://x.com/intent/post?url={u}&text={t}'],
        'whatsapp' => ['WhatsApp', 'https://wa.me/?text={t}%20{u}'], 'reddit' => ['Reddit', 'https://www.reddit.com/submit?url={u}&title={t}'],
        'threads' => ['Threads', 'https://www.threads.net/intent/post?text={t}%20{u}'], 'mail' => ['Email', 'mailto:?subject={t}&body={u}']];
    echo '<section class="ad ad-share"><div class="ad-wrap ad-share__in"><h2>' . esc_html($s['title']) . '</h2><div class="ad-share__row">';
    foreach ($nets as $k => $n) echo '<a class="ad-round" data-share="' . esc_attr($n[1]) . '" href="#" target="_blank" rel="noopener" aria-label="Share on ' . esc_attr($n[0]) . '" title="' . esc_attr($n[0]) . '">' . ads_icon($k) . '</a>';
    echo '<button type="button" class="ad-round ad-share__copy" aria-label="Copy link" title="Copy link">' . ads_icon('link') . '</button></div></div></section>';
}

function ads_r_footer($s) {
    $soc = ['facebook' => 'Facebook', 'youtube' => 'YouTube', 'instagram' => 'Instagram', 'tiktok' => 'TikTok', 'x' => 'X'];
    echo '<footer class="ad ad-footer"><div class="ad-runway" aria-hidden="true"></div><div class="ad-wrap"><div class="ad-footer__top"><div><a class="ad-logo" href="#top">' . ads_logo() . '</a><p>' . esc_html($s['tagline']) . '</p></div>';
    echo '<nav class="ad-footer__links" aria-label="More">';
    foreach ([['Buildings', '#buildings'], ['People', '#people'], ['Four ways to play', '#play'], ['Technology', '#tech'], ['Flight plan', '#route'], ['Get notified', '#buy']] as $l) echo '<a href="' . esc_url($l[1]) . '">' . esc_html($l[0]) . '</a>';
    echo '</nav><div class="ad-footer__social">';
    foreach ($soc as $k => $label) { $u = ads_opt($k); echo '<a class="ad-round" href="' . esc_url($u ?: '#') . '"' . ($u ? ' target="_blank" rel="noopener"' : ' aria-disabled="true"') . ' aria-label="' . esc_attr($label) . '" title="' . esc_attr($label) . '">' . ads_icon($k) . '</a>'; }
    echo '</div></div><div class="ad-footer__bottom"><span>© ' . esc_html(function_exists('wp_date') ? wp_date('Y') : date('Y')) . ' ' . esc_html(ads_opt('holder')) . '. All rights reserved.</span><span>' . esc_html($s['note']) . '</span></div></div></footer>';
}

// #endregion
