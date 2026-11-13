<?php
/** Settings → Airport Dynasty Pro website: social links, the buy button (WooCommerce product), copyright name, pointer, pages. */
if (!defined('ABSPATH')) exit;

// #region ── Settings page ──

add_action('admin_menu', function () {
    add_options_page('Airport Dynasty Pro website', 'Airport Dynasty Pro website', 'manage_options', 'airport-dynasty-site', 'ads_settings_page');
});

add_action('admin_init', function () {
    register_setting('ads', 'ads_settings', [
        'type' => 'array',
        'sanitize_callback' => function ($in) {
            $out = [];
            foreach (['facebook', 'youtube', 'instagram', 'tiktok', 'x', 'buy_url'] as $k) $out[$k] = esc_url_raw(trim($in[$k] ?? ''));
            $out['product_id'] = (string) absint($in['product_id'] ?? 0 ?: 0);
            if ($out['product_id'] === '0') $out['product_id'] = '';
            $out['holder'] = sanitize_text_field($in['holder'] ?? 'Airport Dynasty Pro');
            $out['pointer'] = empty($in['pointer']) ? '0' : '1';
            return $out;
        },
    ]);
    if (isset($_POST['ads_create']) && check_admin_referer('ads_create') && current_user_can('manage_options')) {
        ads_create_pages(!empty($_POST['ads_replace']));
        wp_safe_redirect(admin_url('options-general.php?page=airport-dynasty-site&created=1'));
        exit;
    }
});

function ads_settings_page() {
    $o = ads_opt();
    $f = function ($k, $label, $help = '') use ($o) {
        printf('<tr><th scope="row"><label for="ads_%1$s">%2$s</label></th><td><input class="regular-text" type="url" id="ads_%1$s" name="ads_settings[%1$s]" value="%3$s" placeholder="https://">%4$s</td></tr>',
            esc_attr($k), esc_html($label), esc_attr($o[$k]), $help ? '<p class="description">' . esc_html($help) . '</p>' : '');
    };
    echo '<div class="wrap"><h1>Airport Dynasty Pro website</h1>';
    if (!empty($_GET['created'])) echo '<div class="notice notice-success"><p>The page is ready — see below.</p></div>';
    echo '<form method="post" action="options.php">';
    settings_fields('ads');
    echo '<h2>Buying the game</h2><table class="form-table">';
    $woo = class_exists('WooCommerce');
    printf('<tr><th scope="row"><label for="ads_product_id">WooCommerce product ID</label></th><td><input class="small-text" type="number" min="0" id="ads_product_id" name="ads_settings[product_id]" value="%s"> %s'
        . '<p class="description">The game\'s product in WooCommerce (Products → hover the product → ID). Empty = the buttons say “Coming soon” and lead to the launch-day email list. With a product set, the buttons show its price and put it in the cart.</p></td></tr>',
        esc_attr($o['product_id']), $woo ? '' : '<strong>WooCommerce is not active yet.</strong>');
    $f('buy_url', 'Other shop link (optional)', 'Used only when no WooCommerce product is set, e.g. a Steam page.');
    echo '</table><h2>Links</h2><p>Leave a link empty for now: the icon still shows, and works as soon as you add the link here.</p><table class="form-table">';
    $f('facebook', 'Facebook page');
    $f('youtube', 'YouTube channel');
    $f('instagram', 'Instagram');
    $f('tiktok', 'TikTok');
    $f('x', 'X');
    printf('<tr><th scope="row"><label for="ads_holder">Copyright name</label></th><td><input class="regular-text" id="ads_holder" name="ads_settings[holder]" value="%s"><p class="description">Footer: © (current year, updated automatically) followed by this name.</p></td></tr>', esc_attr($o['holder']));
    printf('<tr><th scope="row">Glowing pointer</th><td><label><input type="checkbox" name="ads_settings[pointer]" value="1" %s> The glowing mouse pointer on the website (computers with a mouse only)</label></td></tr>', checked($o['pointer'], '1', false));
    echo '</table>';
    submit_button();
    echo '</form><hr><h2>Pages</h2><ul>';
    $ids = (array) get_option('ads_pages', []);
    foreach (ads_page_defs() as $key => $def) {
        $id = $ids[$key] ?? 0;
        if ($id && get_post_status($id)) printf('<li><strong>%s</strong> — <a href="%s">view</a> · <a href="%s">edit with Elementor</a></li>', esc_html($def['title']), esc_url(get_permalink($id)), esc_url(admin_url('post.php?post=' . $id . '&action=elementor')));
        else printf('<li><strong>%s</strong> — not created yet</li>', esc_html($def['title']));
    }
    echo '</ul><form method="post">';
    wp_nonce_field('ads_create');
    echo '<p><label><input type="checkbox" name="ads_replace" value="1"> Replace the existing page with a fresh copy (your changes in Elementor on it are lost)</label></p>';
    echo '<p><button class="button button-secondary" name="ads_create" value="1">Create the pages</button></p></form>';
    ads_signups_admin();
    echo '</div>';
}

// #endregion
