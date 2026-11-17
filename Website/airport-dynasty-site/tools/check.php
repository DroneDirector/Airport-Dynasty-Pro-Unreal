<?php
namespace {
// Run: php tools/check.php .   (from the plugin folder; prints NO WARNINGS OR ERRORS when every widget renders cleanly)
// Loads the plugin outside WordPress with strict errors, registers every Elementor widget against stub classes,
// then renders each widget from the settings Elementor would give it (control defaults, repeater rows).
error_reporting(E_ALL);
$ERR = [];
set_error_handler(function ($no, $str, $file, $line) use (&$ERR) { $ERR[] = basename($file) . ":$line $str"; return true; });
define('ABSPATH', '/tmp/'); define('ELEMENTOR_VERSION', '3.25.0');
$P = $argv[1];
foreach (['esc_html','esc_attr','esc_url','esc_textarea','sanitize_text_field','wp_strip_all_tags'] as $f) eval("function $f(\$s){ return htmlspecialchars((string)\$s, ENT_QUOTES, 'UTF-8'); }");
function esc_attr__($s){return $s;} function __($s){return $s;} function esc_html__($s){return $s;} function _e($s){echo $s;}
function wp_kses_post($s){return (string)$s;} function wp_json_encode($v){return json_encode($v);} function wp_slash($v){return $v;}
function plugin_dir_path($f){return dirname($f).'/';} function plugin_dir_url($f){return 'https://airportdynasty.com/wp-content/plugins/airport-dynasty-site/';}
$OPTS = []; function get_option($k,$d=false){global $OPTS; return $OPTS[$k] ?? $d;} function update_option($k,$v){global $OPTS; $OPTS[$k]=$v; return true;} function add_option($k,$v){return update_option($k,$v);}
function add_action(){ } function add_filter(){ } function add_shortcode(){ } function register_activation_hook(){ } function register_deactivation_hook(){ } function do_shortcode($s){return $s;}
function home_url($p=''){return 'https://airportdynasty.com'.$p;} function admin_url($p=''){return 'https://airportdynasty.com/wp-admin/'.$p;}
function wp_create_nonce(){return 'n';} function wp_date($f){return date($f);} function get_post_status(){return false;} function get_permalink(){return '#';}
function wp_rand(){return mt_rand();} function is_singular(){return true;} function get_queried_object_id(){return 1;} function get_post_meta(){return '';}
function current_user_can(){return true;} function did_action(){return 1;} function wp_get_attachment_image_url(){return '';} function wp_get_attachment_url(){return '';}
function get_bloginfo($k=''){return 'Airport Dynasty Pro';} function is_admin(){return false;} function wp_upload_dir(){return ['basedir'=>'/tmp','baseurl'=>'/']; }
function load_plugin_textdomain(){} function apply_filters($t,$v){return $v;} function do_action(){} function wp_unslash($v){return $v;} function sanitize_key($s){return strtolower(preg_replace('/[^a-z0-9_\-]/i','',$s));}
function esc_url_raw($s){return $s;} function absint($v){return abs((int)$v);} function wp_parse_args($a,$d=[]){return array_merge($d,(array)$a);}
}
// Elementor stubs
namespace Elementor {
class Controls_Manager { const TEXT='text'; const TEXTAREA='textarea'; const MEDIA='media'; const SELECT='select'; const REPEATER='repeater'; const TAB_CONTENT='content'; const TAB_STYLE='style'; const COLOR='color'; const SLIDER='slider'; }
class Group_Control_Typography { static function get_type(){ return 'typography'; } }
class Repeater { public $c=[]; function add_control($id,$a){ $this->c[$id]=$a; } function get_controls(){ return $this->c; } }
abstract class Widget_Base { public $controls=[]; public $sec=0; public $grp=[]; function start_controls_section($id,$a){ if(!isset($a['tab'])) trigger_error('section without tab'); $this->sec++; } function end_controls_section(){}
  function add_group_control($t,$a){ if(empty($a['name'])||empty($a['selector'])) trigger_error('bad group control'); $this->grp[]=$a['name']; }
  function add_responsive_control($id,$a){ if(empty($a['selectors'])) trigger_error('no selectors'); $this->controls[$id]=$a+['default'=>'']; }
  function add_control($id,$a){ $this->controls[$id]=$a; }
  function get_settings_for_display(){ $s=[]; foreach($this->controls as $id=>$a){ $s[$id]=$a['default'] ?? ''; if(($a['type']??'')==='repeater'){ foreach($s[$id] as &$row){ foreach($a['fields'] as $fid=>$fa){ if(!array_key_exists($fid,$row)) $row[$fid]=$fa['default'] ?? ''; } $row['_id']='x'; } } } return $s; }
  function run(){ $this->register_controls(); ob_start(); $this->render(); return ob_get_clean(); } }
}
namespace {
require $GLOBALS['P'] . '/airport-dynasty-site.php';
require_once ADS_DIR . 'includes/widgets.php';
$out = '';
foreach (ads_widget_classes() as $cls) { $w = new $cls(); $html = $w->run(); $out .= $html; echo str_pad($cls, 18), strlen($html), " bytes\n"; }
// the page builder too
require_once ADS_DIR . 'includes/pages.php';
$data = ads_elementor_data(ads_page_defs()['home']['widgets']);
echo "page widgets: ", count($data), "\n";
echo count($ERR) ? "ERRORS:\n" . implode("\n", array_unique($ERR)) . "\n" : "NO WARNINGS OR ERRORS\n";
}
