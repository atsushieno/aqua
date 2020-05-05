# aria2web README

`aria2web` is a simple JS+HTML page that brings SFZ [ARIA extensions](https://sfzformat.com/extensions/aria/xml_instrument_bank) XML instrument bank to Web (HTML+SVG) using [webaudio-controls](https://github.com/g200kg/webaudio-controls/). This repository contains copies of JS files from webaudio-controls project.

An online demo UI is available at: https://aria2web.firebaseapp.com/

Right now we use [Vue.js](https://vuejs.org/) but the templating part is really simple so it can be anything. Maybe we don't even need any.

There are handful of great instruments with ARIA UI such as  [Unreal Instruments](https://unreal-instruments.wixsite.com/unreal-instruments). In this PoC repository we import these instruments:

- [UI Standard Guitar](https://unreal-instruments.wixsite.com/unreal-instruments/standard-guitar) 
- [UI METAL GTX](https://unreal-instruments.wixsite.com/unreal-instruments/metal-gtx)
- [UI 1912](https://unreal-instruments.wixsite.com/unreal-instruments/1912)
- [karoryfer-bigcat.cello](https://github.com/sfzinstruments/karoryfer-bigcat.cello)

## Embedded hosting

aria2web is designed to be used for audio plugin UI. `aria2web-host.c` is a proof-of-concept HTTP server and WebView app that shows the program (instrument) list like a native UI using [zserge/webview](https://github.com/zserge/webview) and [jeremycw/httpserver.h](https://github.com/jeremycw/httpserver.h). So far it only logs note on/off and control changes from the UI, but you would get the basic concept on how it could be used.

## lv2 plugin UI

`aria2web-lv2ui` is an UI implementation for LV2 plugins. The plan is to provide a fully-functional SFZ sampler UI using [sfztools/sfizz](https://github.com/sfztools/sfizz/). Though unlike sfizz itself, aria2web [does not support run-time UI loading](https://github.com/atsushieno/aria2web/issues/3) (the HTML pages must be pre-generated) yet.

## Licenses

- My code (`aria2web*`) is available under the MIT License.
- webaudio-controls, webcomponents-lite, and Vue.js are distributed by each developers under their respective licenses (Apache, BSD-like, MIT).
- `httpserver.h` and `webview` are available under the MIT license.
- UI METAL GTX, UI Standard Guitar, and UI 1912 are freely available like public domain (no credits required: `＊ライセンスについて ・ライセンスフリーです ・クレジット表記は不要です`).
- karoryfer-bigcat.cello is under CC-BY 4.0 License.
