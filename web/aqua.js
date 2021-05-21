Aqua = {};
Aqua.Config = {};
Aqua.Config.BankXmlFiles = [];

if (typeof(AquaInterop) != "undefined") {
	Aqua.notifyInitialized = function() { AquaInterop.onInitialized(); }
	Aqua.notifyControlChange = function(c, v) { AquaInterop.onControlChange(c,v); }
	Aqua.notifyNoteMessage = function(s, n) { AquaInterop.onNote(s,n); }
	Aqua.notifyChangeProgram = function(sfz) { AquaInterop.onChangeProgram(sfz); }
	Aqua.getLocalInstruments = function() {
		AquaInterop.onGetLocalInstruments();
		if (Aqua.Config.BankXmlFiles.length == 0) {
			console.log("No local instruments are found. Showing demo UI");
			Aqua.Config.BankXmlFiles = PresetBankXmlFiles;
			onLocalBankFilesUpdated();
		}
	}
} else {
	if (typeof(AquaInitializedCallback) != "undefined")
		Aqua.notifyInitialized = AquaInitializedCallback;
	else
		Aqua.notifyInitialized = function() {
			console.log("aqua JS initialized");
		};
	if (typeof(AquaControlChangeCallback) != "undefined")
		Aqua.notifyControlChange = AquaControlChangeCallback;
	else
		Aqua.notifyControlChange = function(controlId, value) {
			console.log("change event CC: " + controlId + " = " + value);
		};
	if (typeof(AquaNoteCallback) != "undefined")
		Aqua.notifyNoteMessage = AquaNoteCallback;
	else
		Aqua.notifyNoteMessage = function(state, key) {
			console.log("note event: " + state + " " + key);
		};
	if (typeof(AquaChangeProgramCallback) != "undefined")
		Aqua.notifyChangeProgram = AquaChangeProgramCallback;
	else
		Aqua.notifyChangeProgram = function(sfz) {
			console.log("change program event: " + sfz);
		};
	if (typeof(AquaGetLocalInstrumentsCallback) != "undefined")
		Aqua.getLocalInstruments = AquaGetLocalInstrumentsCallback;
	else {
		Aqua.getLocalInstruments = function() {
			console.log("No local instruments are found. Showing demo UI");
			Aqua.Config.BankXmlFiles = PresetBankXmlFiles;
			onLocalBankFilesUpdated();
		}
	}
}

function setupWebAudioControlChangeEvent(e) {
	e.addEventListener("change", () => {
		Aqua.notifyControlChange(
			Number(e.getAttribute("a2w-control")),
			e.value);
		});
}

function setupWebAudioNoteEvent(e) {
	e.addEventListener("change", (evt) => {
		Aqua.notifyNoteMessage(
			evt.note[0],
			evt.note[1]);
		});
}

function setupWebAudioControlEvents() {
	[].slice.call(document.getElementsByTagName("webaudio-knob")).map(setupWebAudioControlChangeEvent);
	[].slice.call(document.getElementsByTagName("webaudio-switch")).map(setupWebAudioControlChangeEvent);
	[].slice.call(document.getElementsByTagName("webaudio-slider")).map(setupWebAudioControlChangeEvent);
		
	[].slice.call(document.getElementsByTagName("webaudio-keyboard")).map(setupWebAudioNoteEvent);
}
