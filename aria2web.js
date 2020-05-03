// TODO: We are still unsure if this can be hooked at embedded environment. If not, try to move it to index.html.
Aria2Web = {};
if (typeof(ControlChangeCallback) != "undefined")
	Aria2Web.notifyControlChange = ControlChangeCallback;
else
	Aria2Web.notifyControlChange = function(instrumentID, controlId, value) {
		console.log(instrumentID + " : change event CC: " + controlId + " = " + value);
	};
if (typeof(NoteCallback) != "undefined")
	Aria2Web.notifyNoteMessage = NoteCallback;
else
	Aria2Web.notifyNoteMessage = function(instrumentID, state, key) {
		console.log(instrumentID + " : note event: " + state + " " + key);
	};

function setupWebAudioControlChangeEvent(e) {
	e.addEventListener("change", () => {
		Aria2Web.notifyControlChange(
			document.Aria2WebInstrument,
			Number(e.getAttribute("a2w-control")),
			e.value);
		});
}

function setupWebAudioNoteEvent(e) {
	e.addEventListener("change", (evt) => {
		Aria2Web.notifyNoteMessage(
			document.Aria2WebInstrument,
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
