<?php
/** The page and how they are built from the Airport Dynasty widgets. */
if (!defined('ABSPATH')) exit;

require_once ADS_DIR . 'includes/content.php';

/** Each page: title, slug and its widgets in order (widget name => settings; empty settings = the widget's defaults). */
function ads_page_defs() {
    $w = function ($k) { return ['ads-' . $k, []]; };
    return [
        'home' => ['title' => 'Airport Dynasty Pro', 'slug' => 'airport-dynasty-pro', 'widgets' => array_map($w,
            ['nav', 'hero', 'ticker', 'stats', 'buildings', 'holo', 'money', 'passes', 'video', 'gallery', 'tech', 'route', 'buy', 'share', 'footer'])],
    ];
}

function ads_eid() { return substr(md5(uniqid('', true) . wp_rand()), 0, 7); }

/** Elementor data for a page: one full-width container per widget, no padding or gaps. */
function ads_elementor_data($widgets) {
    $out = [];
    foreach ($widgets as $w) {
        list($type, $settings) = $w;
        // Repeaters need an _id per row.
        foreach ($settings as $k => $v) {
            if (is_array($v) && isset($v[0]) && is_array($v[0])) {
                foreach ($settings[$k] as $i => $row) $settings[$k][$i]['_id'] = ads_eid();
            }
        }
        $zero = ['unit' => 'px', 'top' => '0', 'right' => '0', 'bottom' => '0', 'left' => '0', 'isLinked' => true];
        $out[] = [
            'id' => ads_eid(), 'elType' => 'container', 'isInner' => false,
            'settings' => ['content_width' => 'full', 'padding' => $zero, 'margin' => $zero,
                'flex_gap' => ['column' => '0', 'row' => '0', 'isLinked' => true, 'unit' => 'px', 'size' => 0]],
            'elements' => [[
                'id' => ads_eid(), 'elType' => 'widget', 'widgetType' => $type, 'settings' => (object) $settings, 'elements' => [],
            ]],
        ];
    }
    return $out;
}

/**
 * Creates the pages (or only the missing ones). With $replace, existing pages get fresh content.
 * The home page becomes the site's front page if the site still shows the latest posts there.
 */
function ads_create_pages($replace) {
    $ids = (array) get_option('ads_pages', []);
    foreach (ads_page_defs() as $key => $def) {
        $id = $ids[$key] ?? 0;
        $exists = $id && get_post_status($id) && get_post_status($id) !== 'trash';
        if ($exists && !$replace) continue;
        if (!$exists) {
            $id = wp_insert_post(['post_type' => 'page', 'post_status' => 'publish', 'post_title' => $def['title'], 'post_name' => $def['slug'], 'post_content' => '']);
            if (is_wp_error($id) || !$id) continue;
            $ids[$key] = $id;
        }
        update_post_meta($id, '_elementor_edit_mode', 'builder');
        update_post_meta($id, '_elementor_template_type', 'wp-page');
        if (defined('ELEMENTOR_VERSION')) update_post_meta($id, '_elementor_version', ELEMENTOR_VERSION);
        update_post_meta($id, '_wp_page_template', 'elementor_canvas');
        update_post_meta($id, '_elementor_data', wp_slash(wp_json_encode(ads_elementor_data($def['widgets']))));
        update_post_meta($id, '_ads_page', $key);
        delete_post_meta($id, '_elementor_css');
    }
    update_option('ads_pages', $ids);
    if (get_option('show_on_front') === 'posts' && !empty($ids['home'])) {
        update_option('show_on_front', 'page');
        update_option('page_on_front', $ids['home']);
    }
    if (class_exists('\Elementor\Plugin')) {
        try { \Elementor\Plugin::$instance->files_manager->clear_cache(); } catch (\Throwable $e) {}
    }
}
