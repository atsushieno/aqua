Aria2Web = {};
Aria2Web.Config = {};
Aria2Web.Config.BankXmlFiles = [];

if (typeof(Aria2WebInitializedCallback) != "undefined")
	Aria2Web.notifyInitialized = Aria2WebInitializedCallback;
else
	Aria2Web.notifyInitialized = function() {
		console.log("aria2web JS initialized");
	};
if (typeof(Aria2WebControlChangeCallback) != "undefined")
	Aria2Web.notifyControlChange = Aria2WebControlChangeCallback;
else
	Aria2Web.notifyControlChange = function(controlId, value) {
		console.log("change event CC: " + controlId + " = " + value);
	};
if (typeof(Aria2WebNoteCallback) != "undefined")
	Aria2Web.notifyNoteMessage = Aria2WebNoteCallback;
else
	Aria2Web.notifyNoteMessage = function(state, key) {
		console.log("note event: " + state + " " + key);
	};
if (typeof(Aria2WebChangeProgramCallback) != "undefined")
	Aria2Web.notifyChangeProgram = Aria2WebChangeProgramCallback;
else
	Aria2Web.notifyChangeProgram = function(sfz) {
		console.log("change program event: " + sfz);
	};

function setupWebAudioControlChangeEvent(e) {
	e.addEventListener("change", () => {
		Aria2Web.notifyControlChange(
			Number(e.getAttribute("a2w-control")),
			e.value);
		});
}

function setupWebAudioNoteEvent(e) {
	e.addEventListener("change", (evt) => {
		Aria2Web.notifyNoteMessage(
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
