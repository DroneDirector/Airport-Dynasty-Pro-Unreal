<?php
/**
 * Plugin Name: Airport Dynasty Pro Website
 * Description: The Airport Dynasty Pro website for Elementor: the game's showcase page with its own look (dark glass, amber glow, glowing pointer), every text and picture editable in Elementor, a launch-day email list and a WooCommerce buy button.
 * Version: 1.0.0
 * Requires at least: 6.0
 * Requires PHP: 7.4
 * Requires Plugins: elementor
 * Author: Airport Dynasty Pro
 * License: GPL-2.0-or-later
 * Text Domain: airport-dynasty-site
 */

if (!defined('ABSPATH')) exit;

define('ADS_VERSION', '1.0.0');
define('ADS_FILE', __FILE__);
define('ADS_DIR', plugin_dir_path(__FILE__));
define('ADS_URL', plugin_dir_url(__FILE__));

require_once ADS_DIR . 'includes/settings.php';
require_once ADS_DIR . 'includes/pages.php';
require_once ADS_DIR . 'includes/signups.php';

// #region ── Settings and helpers ──

/** The fonts, shipped with the plugin (SIL Open Font License). */
function ads_font_faces() {
    $f = ADS_URL . 'assets/fonts/';
    $o = '@font-face{font-family:"AD Inter";src:url("' . esc_url($f . 'inter-var.woff') . '") format("woff");font-weight:100 900;font-display:swap}';
    foreach ([600, 700, 800] as $w) $o .= '@font-face{font-family:"AD Barlow";src:url("' . esc_url($f . 'barlow-condensed-latin-' . $w . '-normal.woff2') . '") format("woff2");font-weight:' . $w . ';font-display:swap}';
    return $o;
}

/** Plugin settings (Settings → Airport Dynasty Pro website), with defaults. */
function ads_opt($key = null) {
    $defaults = [
        'facebook'   => '',
        'youtube'    => '',
        'instagram'  => '',
        'tiktok'     => '',
        'x'          => '',
        'product_id' => '',   // WooCommerce product of the game; empty = "Coming soon"
        'buy_url'    => '',   // or any other shop link (used when no product is set)
        'holder'     => 'Airport Dynasty Pro',
        'pointer'    => '1',
    ];
    $o = wp_parse_args((array) get_option('ads_settings', []), $defaults);
    return $key === null ? $o : ($o[$key] ?? '');
}

/** Link of one of the plugin's pages ('home'). */
function ads_page_url($key) {
    $ids = (array) get_option('ads_pages', []);
    if (!empty($ids[$key]) && get_post_status($ids[$key])) return get_permalink($ids[$key]);
    return $key === 'home' ? home_url('/') : '#';
}

/** [airport_dynasty_year] → the current year, worked out on every page load. */
add_shortcode('airport_dynasty_year', function () { return esc_html(wp_date('Y')); });

// #endregion

// #region ── Styles, scripts and the dark page look ──

add_action('wp_enqueue_scripts', function () {
    wp_register_style('ads-site', ADS_URL . 'assets/css/site.css', [], ADS_VERSION);
    wp_add_inline_style('ads-site', ads_font_faces());
    wp_register_script('ads-site', ADS_URL . 'assets/js/site.js', [], ADS_VERSION, true);
    wp_localize_script('ads-site', 'ADS', [
        'pointer' => ads_opt('pointer') === '1',
        'copied'  => __('Link copied', 'airport-dynasty-site'),
        'ajax'    => admin_url('admin-ajax.php'),
        'nonce'   => wp_create_nonce('ads_notify'),
        'thanks'  => __('Thank you! We’ll email you the day it launches.', 'airport-dynasty-site'),
        'bad'     => __('Please check the email address.', 'airport-dynasty-site'),
        'fail'    => __('That didn’t work, please try again in a moment.', 'airport-dynasty-site'),
    ]);
    if (ads_is_site_page()) { wp_enqueue_style('ads-site'); wp_enqueue_script('ads-site'); }
});

/** True on the plugin's own pages (they get the dark look, the pointer and the scroll-to-top button). */
function ads_is_site_page() {
    if (!is_singular()) return false;
    return (bool) get_post_meta(get_queried_object_id(), '_ads_page', true);
}

add_filter('body_class', function ($c) { if (ads_is_site_page()) $c[] = 'ad-page'; return $c; });
/** The pages are dark already: browsers' own dark modes must not recolour them. */
add_action('wp_head', function () {
    if (!ads_is_site_page()) return;
    echo '<meta name="color-scheme" content="light dark">' . "\n";
    echo '<meta name="supported-color-schemes" content="dark">' . "\n";
    echo '<meta name="darkreader-lock">' . "\n";
    echo '<meta name="theme-color" content="#07080a">' . "\n";
}, 1);

/** Scroll-to-top button on every plugin page (the pointer is drawn by site.js). */
add_action('wp_footer', function () {
    if (!ads_is_site_page()) return;
    echo '<button class="ad-top" type="button" aria-label="' . esc_attr__('Scroll to the top', 'airport-dynasty-site') . '"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 15.5v-2L13.5 9V4.5a1.5 1.5 0 0 0-3 0V9L3 13.5v2l7.5-2.2V18l-2 1.5V21l3.5-1 3.5 1v-1.5l-2-1.5v-4.7z"/></svg></button>';
});

// #endregion

// #region ── Elementor ──

add_action('elementor/elements/categories_registered', function ($em) {
    $em->add_category('airport-dynasty', ['title' => 'Airport Dynasty Pro', 'icon' => 'eicon-globe']);
});

add_action('elementor/widgets/register', function ($wm) {
    require_once ADS_DIR . 'includes/widgets.php';
    foreach (ads_widget_classes() as $cls) $wm->register(new $cls());
});

/** Elementor's editor preview also needs the styles. */
add_action('elementor/preview/enqueue_styles', function () {
    wp_enqueue_style('ads-site', ADS_URL . 'assets/css/site.css', [], ADS_VERSION);
    wp_add_inline_style('ads-site', ads_font_faces());
});

add_action('admin_notices', function () {
    if (did_action('elementor/loaded') || !current_user_can('activate_plugins')) return;
    echo '<div class="notice notice-warning"><p><strong>Airport Dynasty Pro website</strong> needs the free <strong>Elementor</strong> plugin. Install and activate Elementor, then open Settings → Airport Dynasty Pro website and press “Create the pages”.</p></div>';
});

register_activation_hook(__FILE__, function () {
    ads_create_pages(false);
});

// #endregion
