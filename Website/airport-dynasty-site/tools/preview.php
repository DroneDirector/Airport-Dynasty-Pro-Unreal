<?php
/** Builds a stand-alone preview of the page (preview/index.html) from the same section code, outside WordPress: php tools/preview.php */
define('ABSPATH', __DIR__);
define('ADS_DIR', dirname(__DIR__) . '/');
define('ADS_URL', '');
function esc_html($s) { return htmlspecialchars((string) $s, ENT_QUOTES, 'UTF-8'); }
function esc_attr($s) { return esc_html($s); }
function esc_url($s) { return esc_html($s); }
function wp_kses_post($s) { return (string) $s; }
function wp_json_encode($v) { return json_encode($v, JSON_UNESCAPED_UNICODE); }
function ads_opt($k = null) { $o = ['holder' => 'Airport Dynasty Pro', 'product_id' => '', 'buy_url' => '', 'facebook' => '', 'youtube' => '', 'instagram' => '', 'tiktok' => '', 'x' => '']; return $k ? ($o[$k] ?? '') : $o; }
function ads_page_url($k) { return '#top'; }
require ADS_DIR . 'includes/content.php';
require ADS_DIR . 'includes/render.php';
$c = ads_content();
$v = ['eyebrow' => 'The trailer', 'title' => 'See it move.', 'text' => 'Sunrise over the apron, a storm rolling in, the evening rush at the gates.', 'link' => '', 'soon' => 'Trailer coming soon'];
$fonts = '';
foreach ([600, 700, 800] as $w) $fonts .= '@font-face{font-family:"AD Barlow";src:url("assets/fonts/barlow-condensed-latin-' . $w . '-normal.woff2") format("woff2");font-weight:' . $w . ';font-display:swap}';
$fonts .= '@font-face{font-family:"AD Inter";src:url("assets/fonts/inter-var.woff") format("woff");font-weight:100 900;font-display:swap}';
ob_start();
ads_r_nav([]); ads_r_hero($c['hero']); ads_r_ticker(['ticker' => $c['ticker']]); ads_r_stats(['items' => $c['stats']]);
ads_r_buildings($c['buildings']); ads_r_holo($c['holo']); ads_r_passes($c['passes']); ads_r_video($v); ads_r_gallery($c['gallery']);
ads_r_tech($c['tech']); ads_r_route($c['route']); ads_r_buy($c['buy']); ads_r_share($c['share']); ads_r_footer($c['footer']);
$body = ob_get_clean();
$top = '<button class="ad-top" type="button" aria-label="Scroll to the top"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 15.5v-2L13.5 9V4.5a1.5 1.5 0 0 0-3 0V9L3 13.5v2l7.5-2.2V18l-2 1.5V21l3.5-1 3.5 1v-1.5l-2-1.5v-4.7z"/></svg></button>';
$cfg = json_encode(['copied' => 'Link copied', 'bad' => 'Please check the email address.', 'preview' => 'Thank you! (Preview: on the real website this signs you up.)']);
$html = '<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Airport Dynasty Pro — run your own airport</title>'
    . '<meta name="description" content="Be the CEO of your own airport in Iceland. Made in Unreal Engine 5. Coming to PC."><meta name="theme-color" content="#04070d">'
    . '<style>' . $fonts . '</style><link rel="stylesheet" href="assets/css/site.css"></head><body class="ad-page">' . $body . $top
    . '<script>window.ADS=' . $cfg . ';</script><script src="assets/js/site.js"></script></body></html>';
@mkdir(dirname(__DIR__) . '/preview');
file_put_contents(dirname(__DIR__) . '/preview/index.html', $html);
echo strlen($html), "\n";
