<?php
/** The launch-day email list: the sign-up form on the page sends addresses here; the settings page lists them for copying. */
if (!defined('ABSPATH')) exit;

// #region ── Saving a sign-up ──

function ads_notify_handler() {
    if (!check_ajax_referer('ads_notify', 'nonce', false)) wp_send_json_error('nonce', 403);
    if (!empty($_POST['website'])) wp_send_json_success();   // a bot filled the hidden field: pretend it worked
    $email = sanitize_email(wp_unslash($_POST['email'] ?? ''));
    if (!is_email($email)) wp_send_json_error('email', 400);
    $list = (array) get_option('ads_signups', []);
    $key = strtolower($email);
    if (!isset($list[$key])) {
        if (count($list) >= 50000) wp_send_json_error('full', 400);
        $list[$key] = ['email' => $email, 'time' => time()];
        update_option('ads_signups', $list, false);
    }
    wp_send_json_success();
}
add_action('wp_ajax_ads_notify', 'ads_notify_handler');
add_action('wp_ajax_nopriv_ads_notify', 'ads_notify_handler');

// #endregion

// #region ── The list in the settings page ──

function ads_signups_admin() {
    if (isset($_POST['ads_clear']) && check_admin_referer('ads_clear') && current_user_can('manage_options')) {
        delete_option('ads_signups');
        echo '<div class="notice notice-success"><p>The list is empty now.</p></div>';
    }
    $list = (array) get_option('ads_signups', []);
    echo '<hr><h2>Launch-day email list (' . count($list) . ')</h2>';
    echo '<p>Everyone who signed up on the website. Copy the addresses into your email program or newsletter service on launch day.</p>';
    $lines = [];
    foreach ($list as $r) $lines[] = $r['email'] . ',' . wp_date('Y-m-d H:i', (int) $r['time']);
    echo '<textarea readonly rows="10" class="large-text code" onclick="this.select()">' . esc_textarea("email,signed up\n" . implode("\n", $lines)) . '</textarea>';
    echo '<form method="post">';
    wp_nonce_field('ads_clear');
    echo '<p><button class="button" name="ads_clear" value="1" onclick="return confirm(\'Empty the list for good?\')">Empty the list</button></p></form>';
}

// #endregion
