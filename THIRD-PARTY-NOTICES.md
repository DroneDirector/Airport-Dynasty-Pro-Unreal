# Third-party notices

Every third-party part used in Airport Dynasty Pro (Unreal Engine 5 edition), its licence and the credit it asks for.
The table below is for the whole game. Data used only for one airport site is in that site's own section at the end,
so a site can be removed together with its notices.

| Part | Licence | Commercial use | Credit |
|---|---|---|---|
| Unreal Engine 5 | Unreal Engine EULA (royalty 5% above $1M gross per product) | Yes | "Made with Unreal Engine" where the EULA asks |
| MetaHuman (Creator, characters, animation curves) | Part of the Unreal Engine EULA (UE 5.6+) | Yes | As the Unreal Engine EULA asks |
| Chromium Embedded Framework (CEF), shipped with Unreal's web browser view (shows the airline game's screens) | BSD 3-clause (Chromium: BSD and its listed third-party licences) | Yes | Licence text kept with the engine's CEF files |
| The airline game's own code and its third-party parts (the Android edition, shown in the web view) | See Airline-Manager-Pro-Babylon/THIRD-PARTY-NOTICES.md | Yes | As listed there |
| Cesium for Unreal 2.31.0 (the globe), by Cesium GS | Apache 2.0 | Yes | Licence text kept with the plugin; "Cesium" shown on screen |
| Copernicus DEM GLO-90 (world elevation) | Copernicus DEM licence (free, incl. commercial, with the credit) | Yes | On screen: "Copernicus DEM GLO-90 © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018 provided under COPERNICUS by the European Union and ESA" |
| Satellite pictures (fallbacks): NASA Global Imagery Browse Services (GIBS): Landsat WELD global mosaic (NASA / USGS) and Blue Marble Next Generation (NASA Earth Observatory) | NASA imagery: free to use, no restrictions; credit requested | Yes | In the game's credit line when in use: "Imagery: NASA Global Imagery Browse Services (GIBS)…" |
| AMD FSR Upscaling plugin 4.1.1 for Unreal Engine (frame generation), by Advanced Micro Devices | MIT (plugin); AMD FidelityFX SDK binary redistribution licence (its libraries: free, binary only, no reverse engineering) | Yes | Copyright notices and licence texts kept with the plugin (Plugins/FSR/LICENSE, Source/fidelityfx-sdk/.../license.md) and shipped with the game |
| Sentinel-2 cloudless 2016 imagery (layer s2cloudless_3857 only), by EOX IT Services GmbH | CC BY 4.0 | Yes | "Sentinel-2 cloudless 2016 (s2maps.eu) by EOX IT Services GmbH, contains modified Copernicus Sentinel data 2016", shown on screen |
| Kokoro-82M v1.0 (the announcer's voice), by hexgrad; its quantised ONNX build from kokoro-onnx by thewh1teagle | Apache 2.0 (model); MIT (ONNX build) | Yes | Licence texts shipped with the game in Content/Voice (LICENSE-Kokoro-Apache-2.0.txt, LICENSE-kokoro-onnx-MIT.txt) |
| misaki British pronunciation dictionary (the announcer's words), by hexgrad; modified: dictionaries merged, default pronunciations only | Apache 2.0 | Yes | Licence text shipped in Content/Voice (LICENSE-misaki-Apache-2.0.txt) |
| ONNX Runtime (runs the voice model), through Unreal Engine's Neural Network Engine | MIT | Yes | Licence text kept with the engine's NNERuntimeORT plugin |
| ESA WorldCover 10 m 2021 v200 (the land cover on the globe, downloaded with the world's terrain: Tools/MakeLandCover.py); modified: the classes turned into natural colours and averaged into picture tiles | CC BY 4.0 | Yes | On screen when shown: "Land cover: ESA WorldCover 2021 © ESA WorldCover project, contains modified Copernicus Sentinel data (2021) processed by the ESA WorldCover consortium, CC BY 4.0" |
| Blender's bundled font (Inter, by Rasmus Andersson), its letter shapes made into the taxi's plate and roof-sign lettering (Art/Taxi) | SIL Open Font License 1.1 | Yes | Licence text kept with Blender; not needed in the game's credits |
| DejaVu Sans Bold and DejaVu Sans (the DejaVu fonts team), the lettering drawn into the start-up splash pictures (Content/Splash, Art/Icon/make_brand_images.py) | Bitstream Vera / DejaVu free licence (any use, including commercial) | Yes | Licence text with the fonts |

## Ground scans - Poly Haven (CC0)
asphalt_04 (Sergej Majboroda, Jenelle van Heerden), asphalt_02 (Rob Tuytel), concrete_floor_worn_001 (Dimitrios Savva,
Rico Cilliers), brushed_concrete_04 (Amal Kumar), aerial_grass_rock (Rob Tuytel), sparse_grass (Amal Kumar), grass_ground (Charlotte Baglioni), from https://polyhaven.com, CC0 1.0 (public domain). The airfield's
asphalt and concrete and the pavement edges (Art/Ground/Scans, Tools/set_ground.py, Tools/set_edge_bands.py).

## Cargo terminal textures - ambientCG and Poly Haven (CC0)
Paint004, Metal011, Metal027, Rubber004 from https://ambientcg.com (Art/GPU/Textures) and asphalt_02,
concrete_floor_worn_001, brushed_concrete_04 from https://polyhaven.com (Art/Ground/Scans), all CC0 1.0 (public domain),
are the base of the cargo terminal's colour, normal and roughness maps (Art/CargoTerminal/make_cargo_textures.py, which
draws the panel joints, seams, door sections, glazing bars and markings on them itself).

## MetaRoad Free - udrive (Ivan Zhukov), Fab Standard License
MetaRoad 3.3.0 (https://www.fab.com/listings/a425c08b-d82a-4b42-ab94-101c3eb23f28), installed to Unreal Engine 5.7 from
the owner's Fab library. Licensed under the Fab Standard License (https://www.fab.com/eula): used to author and bake the
airport's, the road's and the town's road network (Tools/build_roads.py); baked roads ship as static meshes, the plugin
is packed for its run-time road graph only. The plugin itself is not redistributed on its own.

## Apron bus sounds - Freesound (CC0)
Art/Sounds/ApronBus (made by Tools/make_apron_bus_sounds.py): recordings by felix.blume (electric trolleybus), BabelFishTank
(bus door), Filmscore (air brakes), derjuli (electric motor), pkri (scooter motor), tt_runscript (contactors), evsecrets
(EV ready beep), nuckan (pump motor), Disasteradio (reverse beeper), Camilo_Vargas13 (horn), from https://freesound.org,
CC0 1.0 (public domain); each sound's page and what was taken from it is in Art/Sounds/ApronBus/SOURCES.md. The AVAS
tone, door chime, indicator relay, A/C fans, motor tones and door thud are our own synthesis.

## Cargo terminal sounds - Freesound (CC0)
Art/Sounds/CargoTerminal (made by Tools/make_cargo_sounds.py): recordings by kyles (rain on a metal roof, warehouse room
tone), chillaquil (rain under a metal roof), jaegrover (outdoor heating and ventilation unit), HECKFRICKER (ventilation
outlet) and monotraum (office printer), from https://freesound.org, CC0 1.0 (public domain); each sound's page is in
Art/Sounds/CargoTerminal/SOURCES.md. Its doors reuse the fire station's door and the jetway's service door sounds.

## Meshy models (owner's own)
The models listed in Art/MeshyIntake/README.md (lorry, cargo vehicles, aircraft, received 2026-11-26) were made by the
owner with Meshy AI (https://www.meshy.ai) on a paid plan that includes commercial use; the owner holds the rights to
use them in the game. No real brand names, logos or liveries are carried over onto the game models.

## Music
- The title music (title.mp3): made by the owner with Suno AI on a Pro plan, which gives him full ownership and commercial use; safe to use everywhere, including live streams on YouTube and elsewhere (owner, 2026-11-20). Every future soundtrack is listed here with its source and licence.

# Site: Iceland airport (BIAP) - remove with the Iceland site

| Part | Licence | Commercial use | Credit |
|---|---|---|---|
| Faroe Islands elevation model: DSM hæddarmodell 2015 (10 m), by the Faroese Environment Agency, Umhvørvisstovan (foroyakort.fo) | Føroyakort terms (June 2019): free use incl. commercial, with credit | Yes | In the game: "Data from the Faroese Environment Agency, Umhvørvisstovan: DSM hæddarmodell 2015, November 2026"; terms: https://foroyakort.fo/um-foeroyakort/terms-and-conditions-in-english |
| ÍslandsDEM (Iceland elevation model, 10 m), by Náttúrufræðistofnun / Landmælingar Íslands (LMÍ) | CC BY 4.0 | Yes | "ÍslandsDEM © Landmælingar Íslands, CC BY 4.0" |
| ESA WorldCover 10 m 2021 v200 (the sea's outline round the airport, Content/Data/ocean.bin); modified: the open sea traced into a water surface | CC BY 4.0 | Yes | "© ESA WorldCover project 2021 / Contains modified Copernicus Sentinel data (2021) processed by ESA WorldCover consortium" |

# Site: Chablais Airport (Switzerland) - remove with the Chablais site

- Heights: swissALTI3D (0.5 m) and aerial photos: SWISSIMAGE 10 cm - source: swisstopo (Federal Office of Topography),
  open government data, free use including commercial, attribution "© swisstopo". Downloaded 2026-11-20 from data.geo.admin.ch.
- French side of the map: RGE ALTI 1 m heights and BD ORTHO 20 cm photos - source: IGN (Institut national de l'information
  géographique et forestière), Licence Ouverte / Open Licence 2.0 (Etalab), commercial use allowed with attribution "© IGN".
  Downloaded 2026-11-20 from data.geopf.fr.
