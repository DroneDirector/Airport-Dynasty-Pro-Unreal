<?php
/**
 * All the website's starting text and pictures. Everything can be changed later in Elementor (click a section and
 * edit it in the left panel). New screenshots: replace a picture in its section, or add more rows. Lists that are
 * not complete end with "…and much more", so the site never claims to show everything.
 */
if (!defined('ABSPATH')) exit;

function ads_img($file) { return ['url' => ADS_URL . 'assets/img/' . $file, 'id' => '']; }

function ads_content() {
    return [
        // #region ── Hero ──
        'hero' => [
            'sign'   => 'Welcome to Mýrdalssandur',
            'title'  => 'Run your own <em>airport.</em>',
            'text'   => 'Be the CEO of a growing airport on Iceland’s south coast. Expand the terminal, hire crews, plan routes on a fully rendered 3D globe and watch thousands of passengers live their journeys in stunning detail.',
            'film'   => 'Watch the trailer',
            'note'   => 'Made in Unreal Engine 5 · Coming to PC',
            'images' => [
                ['image' => ads_img('dealer_ext.jpg'), 'caption' => 'The car dealership in your home town'],
                ['image' => ads_img('villa_garden.jpg'), 'caption' => 'Your villa, the glacier to the north'],
                ['image' => ads_img('dealer_int.jpg'), 'caption' => 'Inside the showroom'],
                ['image' => ads_img('villa_drive.jpg'), 'caption' => 'Coming home along your own drive'],
            ],
            // The feature slider: one slide per line, ICON|TITLE|TEXT (icons: terminal, globe, people, plane, home, sun, mesh, more)
            'slides' => "terminal|Grow your terminal|Start small, then add new wings piece by piece as more passengers arrive.\nglobe|The whole Earth in 3D|Plan your routes on a fully rendered globe with real terrain, rivers and coastlines.\npeople|Thousands of people|Every passenger has a booking, a mood, money in their pocket and a trip to finish.\nplane|AI airlines fly your routes|You plan the routes and the timetables. Your hired crews do the flying.\nhome|Your villa and your town|Drive home after work to your own villa in a small Icelandic town.\nsun|Real weather, day and night|Sunsets, storms and snow, following the real date and time.\nmesh|Made in Unreal Engine 5|Nanite and Lumen bring film-like detail and light to every corner.\nmore|And much more|New buildings and features join with every update.",
        ],
        // #endregion
        // #region ── Ticker ──
        'ticker' => "Check-in · Security · Gates · Jetways · Control tower · Fire station · Hangars · Car rental · Airlines · Timetables · Crews · The whole Earth · Day and night · Weather · Your villa · Your home town · And much more",
        // #endregion
        // #region ── Numbers ──
        'stats' => [
            ['num' => '5,000', 'label' => 'career ranks, from first day to legend'],
            ['num' => '6.7 km', 'label' => 'of road from the terminal to your town'],
            ['num' => '24 h', 'label' => 'real day and night on the real date'],
            ['num' => '360°', 'label' => 'the whole Earth, fully rendered'],
        ],
        // #endregion
        // #region ── Buildings (wayfinding signs) ──
        'buildings' => [
            'eyebrow' => 'Follow the signs',
            'title'   => 'Buildings you <em>run, walk and grow.</em>',
            'text'    => 'More buildings join with every update. Here’s where things stand.',
            'items'   => [
                ['icon' => 'terminal', 'name' => 'The terminal', 'text' => 'Check-in, security, shops, gates and jetways. Walkable, and it grows with you.', 'status' => 'In the game', 'arrow' => '→'],
                ['icon' => 'tower', 'name' => 'Control tower', 'text' => 'Watch every movement on the field from the top.', 'status' => 'In the game', 'arrow' => '↑'],
                ['icon' => 'hangar', 'name' => 'Hangars', 'text' => 'Where the fleet is serviced and kept flying.', 'status' => 'In the game', 'arrow' => '←'],
                ['icon' => 'fire', 'name' => 'Fire station', 'text' => 'Fire trucks you can climb into and drive yourself.', 'status' => 'In the game', 'arrow' => '↗'],
                ['icon' => 'car', 'name' => 'Car dealership', 'text' => 'Buy and sell cars in your home town.', 'status' => 'Being built', 'arrow' => '→'],
                ['icon' => 'home', 'name' => 'Your villa', 'text' => 'Your own home at the end of its own road.', 'status' => 'Being built', 'arrow' => '↓'],
                ['icon' => 'more', 'name' => 'And much more', 'text' => 'Shops, a petrol station, a hairdresser, the town itself and more on the way.', 'status' => 'Coming', 'arrow' => '…'],
            ],
        ],
        // #endregion
        // #region ── Holographic map (people) ──
        'holo' => [
            'eyebrow' => 'The people',
            'title'   => 'Thousands of lives. <em>Live on your map.</em>',
            'text'    => 'Lifelike people built from MetaHumans, each with a reason to be there. Your holographic map shows them all, floor by floor, and flashes the moment something happens.',
            'points'  => "Every passenger has a booking, a mood, money and a trip to finish\nPilots, cabin crew, ground crew, security, police, cleaners and fire crews, each in a proper uniform\nIncidents blink on the map: a difficult passenger, a guard escort, a lost bag\nStaff work their shifts and drive home to the town afterwards\n…and much more",
        ],
        // #endregion
        // #region ── Four ways to play (features) ──
        'passes' => [
            'eyebrow' => 'What you do',
            'title'   => 'Four ways <em>to play.</em>',
            'items'   => [
                ['name' => 'The airport', 'image' => ads_img('overview.jpg'),
                    'points' => "Walk every corner of your terminal\nExpand it, piece by piece, as traffic grows\nJetways, stands, timetable screens and announcements\nTower, fire station and hangars\nSunsets, storms and snow on the real date\n…and much more"],
                ['name' => 'The people', 'image' => ads_img('dealer_int.jpg'),
                    'points' => "Thousands of passengers with their own trips\nHire, dress and schedule your staff\nSecurity, police and sniffer dogs\nIncidents to handle as they happen\n…and much more"],
                ['name' => 'Airlines & world', 'image' => ads_img('dealer_ext.jpg'),
                    'points' => "Buy, sell or lease aircraft\nCrews, timetables and rival airlines\nPlan routes on a fully rendered 3D globe\nRankings, thousands of awards and your own museum\nShops, car rental and parking income\n…and much more"],
                ['name' => 'Your life', 'image' => ads_img('villa_garden.jpg'),
                    'points' => "Create your character: she or he, looks, a pet, a partner\nYour own villa, garage and car\nDrive the 6.7 km road home to town\nShops, a car dealership and a hairdresser\nRadio and news on the way\n…and much more"],
            ],
        ],
        // #endregion
        // #region ── Pictures ──
        'gallery' => [
            'eyebrow' => 'From the development',
            'title'   => 'A look <em>around.</em>',
            'note'    => 'Work-in-progress pictures. Everything may still change.',
            'shots'   => [
                ['image' => ads_img('dealer_ext.jpg'), 'caption' => 'The car dealership in your home town'],
                ['image' => ads_img('dealer_int.jpg'), 'caption' => 'Inside the showroom: the turntable never stops'],
                ['image' => ads_img('villa_garden.jpg'), 'caption' => 'Your own villa, the glacier to the north'],
                ['image' => ads_img('villa_drive.jpg'), 'caption' => 'Coming home along your own drive'],
                ['image' => ads_img('overview.jpg'), 'caption' => 'The airport, the 6.7 km road and your town'],
                ['image' => ads_img('town.jpg'), 'caption' => 'Your home town, with the road signs to the airport'],
            ],
        ],
        // #endregion
        // #region ── Technology: Nanite and Lumen, look and performance ──
        'tech' => [
            'eyebrow' => 'Under the hood',
            'title'   => 'Unreal Engine 5. <em>What it means for you.</em>',
            'text'    => 'Two of the engine’s features shape how Airport Dynasty Pro looks and runs.',
            'nanite_look' => 'Buildings, aircraft and every bench are made of millions of tiny triangles, film-quality detail. Edges stay round, surfaces stay sharp, from across the apron or a metre away, with no models popping in and out as you move.',
            'nanite_perf' => 'Nanite only draws the detail your screen can actually show and streams the rest as needed. So more detail costs far less than it used to: a huge, detailed airport runs smoothly instead of grinding your PC down.',
            'lumen_look'  => 'Light bounces like real light: sunshine off the floor, through the glass, onto the people. Nothing is painted on in advance, so dawn, sunsets, storms and night shifts all light up the airport the way they would in life, as the sun moves.',
            'lumen_perf'  => 'Real-time light is the most demanding part of the picture, so it scales: the graphics settings let you choose its quality, from lighter on older PCs to full quality on strong ones, and the game checks your PC to suggest a starting point.',
            'note'    => 'Plus MetaHuman people and Virtual Shadow Maps for crisp shadows on every detail.',
        ],
        // #endregion
        // #region ── Roadmap (flight path) ──
        'route' => [
            'eyebrow' => 'Flight plan',
            'title'   => 'Where it’s <em>heading.</em>',
            'items'   => [
                ['when' => 'Now', 'title' => 'The airport comes alive', 'text' => 'Terminal, people, staff and uniforms, airlines, aircraft, timetables, the 3D globe, rankings and awards.'],
                ['when' => 'Next', 'title' => 'Your town and your home', 'text' => 'The town fully built: your villa, the car dealership, shops and the hairdresser. Staff drive home after work.'],
                ['when' => 'Then', 'title' => 'A bigger airport', 'text' => 'Terminal extensions over the new apron, more gates and stands, the fire station and hangars in full use.'],
                ['when' => 'Later', 'title' => 'And much more', 'text' => 'Interact with everything you see, more events and incidents, and new things with every update.'],
            ],
        ],
        // #endregion
        // #region ── Buy, share, footer ──
        'buy' => [
            'title'  => 'Be there on day one.',
            'text'   => 'Airport Dynasty Pro will be sold right here, straight from the developer. Leave your email and we’ll tell you the day it launches.',
            'live'   => 'Airport Dynasty Pro is out now on PC. Buy it here, straight from the developer.',
            'button' => 'Notify me',
            'small'  => 'One email on launch day. Nothing else.',
        ],
        'share'  => ['title' => 'Share Airport Dynasty Pro'],
        'footer' => [
            'tagline' => 'Be the CEO of your own airport in Iceland. Made in Unreal Engine 5. Coming to PC.',
            'note'    => 'Unreal and Unreal Engine are trademarks or registered trademarks of Epic Games, Inc.',
        ],
        // #endregion
    ];
}
