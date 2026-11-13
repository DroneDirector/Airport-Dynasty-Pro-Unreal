<?php
/** The Airport Dynasty Pro Elementor widgets: thin wrappers around the section functions in render.php, with every text, picture and list editable in Elementor's panel. */
if (!defined('ABSPATH')) exit;

use Elementor\Controls_Manager;
use Elementor\Repeater;
use Elementor\Widget_Base;

require_once ADS_DIR . 'includes/content.php';
require_once ADS_DIR . 'includes/render.php';

/** Each widget: name => [title, Elementor icon, renderer, fields, repeaters]. Fields: id => [type, label, default]. */
function ads_widget_defs() {
    $c = ads_content();
    $txt = function ($k, $d) { return ['textarea', $k, $d]; };
    return [
        'nav'       => ['Top bar', 'eicon-nav-menu', 'ads_r_nav', [], []],
        'hero'      => ['Hero and feature slider', 'eicon-banner', 'ads_r_hero',
            ['sign' => ['text', 'Yellow sign', $c['hero']['sign']], 'title' => $txt('Title (<em>word</em> = glowing)', $c['hero']['title']), 'text' => $txt('Text', $c['hero']['text']),
             'film' => ['text', 'Trailer button', $c['hero']['film']], 'note' => ['text', 'Small line under the buttons', $c['hero']['note']],
             'board' => $txt('Departures board: one row per line, FLIGHT|TO|GATE|STATUS', $c['hero']['board'])],
            ['images' => ['Background pictures (they fade one into the next)', ['image' => ['media', 'Picture', ''], 'caption' => ['text', 'Caption', '']], $c['hero']['images'], 'caption']]],
        'ticker'    => ['Ticker line', 'eicon-animation-text', 'ads_r_ticker', ['ticker' => $txt('Text (separate with ·)', $c['ticker'])], []],
        'stats'     => ['Numbers', 'eicon-counter', 'ads_r_stats', [], ['items' => ['Numbers', ['num' => ['text', 'Number', ''], 'label' => ['text', 'Label', '']], $c['stats'], 'num']]],
        'video'     => ['Trailer', 'eicon-youtube', 'ads_r_video',
            ['eyebrow' => ['text', 'Small line above the title', 'The trailer'], 'title' => ['text', 'Title', 'See it move.'], 'text' => $txt('Text', 'Sunrise over the apron, a storm rolling in, the evening rush at the gates.'),
             'link' => ['text', 'YouTube or Vimeo link (empty = “coming soon”)', ''], 'soon' => ['text', 'Text while there is no trailer', 'Trailer coming soon']], []],
        'buildings' => ['Buildings (signs)', 'eicon-sign-out', 'ads_r_buildings',
            ['eyebrow' => ['text', 'Small line above the title', $c['buildings']['eyebrow']], 'title' => $txt('Title', $c['buildings']['title']), 'text' => $txt('Text', $c['buildings']['text'])],
            ['items' => ['Signs (keep “And much more” as the last one)', ['icon' => ['select:terminal|tower|hangar|fire|car|home|plane|more', 'Pictogram', 'terminal'], 'name' => ['text', 'Name', ''], 'text' => ['textarea', 'Text', ''],
                'status' => ['text', 'Status (In the game / Being built / Coming)', 'In the game'], 'arrow' => ['text', 'Arrow', '→']], $c['buildings']['items'], 'name']]],
        'holo'      => ['People (holographic map)', 'eicon-map-pin', 'ads_r_holo',
            ['eyebrow' => ['text', 'Small line above the title', $c['holo']['eyebrow']], 'title' => $txt('Title', $c['holo']['title']), 'text' => $txt('Text', $c['holo']['text']),
             'points' => $txt('Points (one per line; end with …and much more)', $c['holo']['points'])], []],
        'money'     => ['Economy, phone and computer', 'eicon-price-table', 'ads_r_money',
            ['eyebrow' => ['text', 'Small line above the title', $c['money']['eyebrow']], 'title' => $txt('Title', $c['money']['title']), 'text' => $txt('Text', $c['money']['text']),
             'ledger' => $txt('Airport account: one row per line, + or -|WHAT', $c['money']['ledger']), 'points' => $txt('Costs (one per line; end with …and much more)', $c['money']['points']),
             'dev_title' => ['text', 'Phone and computer: title', $c['money']['dev_title']], 'dev_text' => $txt('Phone and computer: text', $c['money']['dev_text']),
             'dev_points' => $txt('Phone and computer: points (end with …and much more)', $c['money']['dev_points'])], []],
        'passes'    => ['Four ways to play', 'eicon-info-box', 'ads_r_passes',
            ['eyebrow' => ['text', 'Small line above the title', $c['passes']['eyebrow']], 'title' => $txt('Title', $c['passes']['title'])],
            ['items' => ['Passes', ['name' => ['text', 'Name', ''], 'image' => ['media', 'Picture', ''],
                'points' => ['textarea', 'Points (one per line; end with …and much more)', '']], $c['passes']['items'], 'name']]],
        'gallery'   => ['Pictures', 'eicon-slider-push', 'ads_r_gallery',
            ['eyebrow' => ['text', 'Small line above the title', $c['gallery']['eyebrow']], 'title' => $txt('Title', $c['gallery']['title']), 'note' => ['text', 'Small note', $c['gallery']['note']]],
            ['shots' => ['Pictures (add, remove, drag to reorder)', ['image' => ['media', 'Picture', ''], 'caption' => ['text', 'Caption', '']], $c['gallery']['shots'], 'caption']]],
        'tech'      => ['Technology (AAA graphics)', 'eicon-code', 'ads_r_tech',
            ['eyebrow' => ['text', 'Small line above the title', $c['tech']['eyebrow']], 'title' => $txt('Title', $c['tech']['title']), 'text' => $txt('Text', $c['tech']['text']),
             'nanite_look' => $txt('Nanite: the look', $c['tech']['nanite_look']), 'nanite_perf' => $txt('Nanite: performance', $c['tech']['nanite_perf']),
             'lumen_look' => $txt('Lumen: the look', $c['tech']['lumen_look']), 'lumen_perf' => $txt('Lumen: performance', $c['tech']['lumen_perf']), 'extras' => $txt('More graphics: one per line, NAME|TEXT', $c['tech']['extras']), 'note' => $txt('Note under the cards', $c['tech']['note'])], []],
        'route'     => ['Flight plan (roadmap)', 'eicon-time-line', 'ads_r_route',
            ['eyebrow' => ['text', 'Small line above the title', $c['route']['eyebrow']], 'title' => $txt('Title', $c['route']['title'])],
            ['items' => ['Stops', ['when' => ['text', 'When', ''], 'title' => ['text', 'Title', ''], 'text' => ['textarea', 'Text', '']], $c['route']['items'], 'title']]],
        'buy'       => ['Buy / launch-day list', 'eicon-cart', 'ads_r_buy',
            ['title' => ['text', 'Title', $c['buy']['title']], 'text' => $txt('Text before launch', $c['buy']['text']), 'live' => $txt('Text when it is on sale', $c['buy']['live']),
             'button' => ['text', 'Sign-up button', $c['buy']['button']], 'small' => ['text', 'Small text under the form', $c['buy']['small']]], []],
        'share'     => ['Share this page', 'eicon-share', 'ads_r_share', ['title' => ['text', 'Title', $c['share']['title']]], []],
        'footer'    => ['Footer', 'eicon-footer', 'ads_r_footer', ['tagline' => ['text', 'Tagline', $c['footer']['tagline']], 'note' => ['text', 'Small note', $c['footer']['note']]], []],
    ];
}

function ads_widget_classes() {
    return ['ADS_W_Nav', 'ADS_W_Hero', 'ADS_W_Ticker', 'ADS_W_Stats', 'ADS_W_Video', 'ADS_W_Buildings', 'ADS_W_Holo', 'ADS_W_Money', 'ADS_W_Passes', 'ADS_W_Gallery', 'ADS_W_Tech', 'ADS_W_Route', 'ADS_W_Buy', 'ADS_W_Share', 'ADS_W_Footer'];
}

abstract class ADS_Base extends Widget_Base {
    protected $key = '';
    protected function def() { return ads_widget_defs()[$this->key]; }
    public function get_name() { return 'ads-' . $this->key; }
    public function get_title() { return 'Airport Dynasty: ' . $this->def()[0]; }
    public function get_icon() { return $this->def()[1]; }
    public function get_categories() { return ['airport-dynasty']; }
    public function get_style_depends() { return ['ads-site']; }
    public function get_script_depends() { return ['ads-site']; }
    public function get_keywords() { return ['airport', 'dynasty', 'game']; }

    protected function add_field($id, $f, $target = null) {
        list($type, $label, $def) = $f;
        $map = ['text' => Controls_Manager::TEXT, 'textarea' => Controls_Manager::TEXTAREA, 'media' => Controls_Manager::MEDIA];
        $args = ['label' => $label, 'default' => $def, 'label_block' => true];
        if (strpos($type, 'select:') === 0) {
            $opts = []; foreach (explode('|', substr($type, 7)) as $o) $opts[$o] = ucfirst($o);
            $args['type'] = Controls_Manager::SELECT; $args['options'] = $opts; $args['label_block'] = false;
        } else {
            $args['type'] = $map[$type];
            if ($type === 'media') $args['dynamic'] = ['active' => true];
            if ($type === 'textarea') $args['rows'] = 5;
        }
        ($target ?: $this)->add_control($id, $args);
    }

    protected function register_controls() {
        list(, , , $fields, $reps) = $this->def();
        $this->start_controls_section('ads_content', ['label' => 'Content', 'tab' => Controls_Manager::TAB_CONTENT]);
        foreach ($fields as $id => $f) $this->add_field($id, $f);
        foreach ($reps as $id => $r) {
            list($label, $rf, $rows, $title) = $r;
            $rep = new Repeater();
            foreach ($rf as $fid => $f) $this->add_field($fid, $f, $rep);
            $this->add_control($id, ['label' => $label, 'type' => Controls_Manager::REPEATER, 'fields' => $rep->get_controls(), 'default' => $rows, 'title_field' => '{{{ ' . $title . ' }}}']);
        }
        $this->end_controls_section();
    }

    protected function render() {
        $fn = $this->def()[2];
        $fn($this->get_settings_for_display());
    }
}

// One small class per widget (the definitions are in ads_widget_defs above).
class ADS_W_Nav extends ADS_Base { protected $key = 'nav'; }
class ADS_W_Hero extends ADS_Base { protected $key = 'hero'; }
class ADS_W_Ticker extends ADS_Base { protected $key = 'ticker'; }
class ADS_W_Stats extends ADS_Base { protected $key = 'stats'; }
class ADS_W_Video extends ADS_Base { protected $key = 'video'; }
class ADS_W_Buildings extends ADS_Base { protected $key = 'buildings'; }
class ADS_W_Holo extends ADS_Base { protected $key = 'holo'; }
class ADS_W_Money extends ADS_Base { protected $key = 'money'; }
class ADS_W_Passes extends ADS_Base { protected $key = 'passes'; }
class ADS_W_Gallery extends ADS_Base { protected $key = 'gallery'; }
class ADS_W_Tech extends ADS_Base { protected $key = 'tech'; }
class ADS_W_Route extends ADS_Base { protected $key = 'route'; }
class ADS_W_Buy extends ADS_Base { protected $key = 'buy'; }
class ADS_W_Share extends ADS_Base { protected $key = 'share'; }
class ADS_W_Footer extends ADS_Base { protected $key = 'footer'; }
