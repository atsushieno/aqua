Aqua = {};
Aqua.Config = {};
Aqua.Config.BankXmlFiles = [];

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
