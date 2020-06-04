PresetBankXmlFiles = [
	"banks/ui-metal-gtx/METAL-GTX.bank.xml",
	"banks/ui-standard-guitar/Standard Guitar.bank.xml",
	"banks/ui-1912/1912.bank.xml",
	"banks/karoryfer-bigcat.cello/Karoryfer x bigcat cello.bank.xml"
];

async function readFileBlobAsText(filePath) {
	var res = await fetch(filePath);
	var blob = await res.blob();
	return new Promise((resolve, reject) => {
		var fr = new FileReader();  
		fr.onload = () => {
			resolve(fr.result);
		};
		fr.onerror = reject;
		fr.readAsText(blob);
	});
}

async function onBodyLoad() {
	// load aria2web.xsl.
	var xslText = await readFileBlobAsText("aria2web.xsl");
	aria2web_stylesheet = new XSLTProcessor();
	var xsltDoc = new DOMParser().parseFromString(xslText, "application/xml");
	aria2web_stylesheet.importStylesheet(xsltDoc.documentElement);

	// prepare bank list.
	loadBankXmlFileList();
}

async function loadBankXmlFileList() {
	if (typeof (GetLocalInstrumentsCallback) != "undefined") {
		GetLocalInstrumentsCallback();
	} else {
		alert("No local instruments are found. Showing demo UI");
		Aria2Web.Config.BankXmlFiles = PresetBankXmlFiles;
		onLocalBankFilesUpdated();
	}
}

// This is called by either explicitly from this script or via C callback.
function onLocalBankFilesUpdated() {
	var bankXmlFiles = Aria2Web.Config.BankXmlFiles;

	for (var i in Aria2Web.Config.BankXmlFiles)
		console.log(Aria2Web.Config.BankXmlFiles[i]);

	results = [];
	
	bankXmlFiles.map(async bankXmlFile => {
		var bankXmlContent = await readFileBlobAsText(bankXmlFile);
		var programList = await processBankXmlContent(bankXmlFile, bankXmlContent);
		results.push(programList);
		if (results.length == bankXmlFiles.length)
			app = new Vue({
				el: '#app',
				data: { programList: results }
			});
	});
}

async function onChooseBankDirectoryCommand() {

	if (typeof (window.chooseFileSystemEntries) != "undefined") {
		window.chooseFileSystemEntries({type: "open-directory"})
			.then(dirent => {
				var dir = getDirectory(dirent.name)
			});
	} else {
		alert("Native file system API is not supported on this Web Browser / WebView.");
	}
}

async function processBankXmlContent(bankXmlFile, bankXmlFragmentTextRaw) {
	// It is actually not a well-formed XML, so it is inappropriate to call it an "XML".
	// It is named so only to reflect the file name...
	var pattern = /<\?xml[\s]+version[\s]*=\"1.0\"[\s]*\?>/;
	var bankXmlFragmentText = bankXmlFragmentTextRaw.replace(pattern, '');

	var doc = new DOMParser().parseFromString("<dummy>" + bankXmlFragmentText + "</dummy>", "application/xml");

	var ariaGuiPath = doc.documentElement.querySelector("AriaGUI").getAttribute("path");
	var ariaGuiPathAbs = new URL(ariaGuiPath, new URL(bankXmlFile, document.baseURI));
	
	var ariaGuiContent = await readFileBlobAsText(ariaGuiPathAbs);
	return await processAriaGuiContent(bankXmlFile, doc, ariaGuiPathAbs, ariaGuiContent);
}

async function processAriaGuiContent(bankXmlFile, doc, ariaGuiPathAbs, ariaGuiDocTextRaw) { // returns Bank{name/thumbnail/programList}
	var ariaGuiDoc = new DOMParser().parseFromString(ariaGuiDocTextRaw, "application/xml");
	var thumbnail = ariaGuiDoc.documentElement.querySelector("StaticImage").getAttribute("image");
	var thumbnailAbs = new URL(thumbnail, ariaGuiPathAbs);

	var programNodes = [].slice.call(doc.documentElement.getElementsByTagName("AriaProgram"));
	var programs = programNodes.map(function(pn) {
		var bankXmlURL = new URL(bankXmlFile, document.baseURI);
		var guiURL = new URL(pn.getAttribute("gui"), bankXmlURL);
		return {
			name : pn.getAttribute("name"),
			source: guiURL.toString()
		};
	});
	var q = doc.documentElement.querySelector("AriaBank");
	var bankName = q.getAttribute("name");
	return {
		name: bankName,
		thumbnail: thumbnailAbs.toString(),
		programList: programs
	};
}

// `<a href="..." target="instFrame">` did not work on the embedded webview, but this simple workaround works.
async function loadProgramXmlOnInstFrame(el) {

	var programXmlFile = el.getAttribute("a2w-source");

	var programXmlContent = await readFileBlobAsText(programXmlFile);
	var sourceDoc = new DOMParser().parseFromString(programXmlContent, "application/xml");
	var resultDoc = aria2web_stylesheet.transformToDocument(sourceDoc);

	var contentDiv = resultDoc.documentElement.querySelector("body > div");

	// It is kind of ugly hack to get those standalone AriaProgram page working, by
	// replacing all those image resource paths.
	var placeholder = document.getElementById("placeholder");
	placeholder.setAttribute("base", programXmlFile);

	var imgs = contentDiv.getElementsByTagName("img");
	[].slice.call(imgs).map(e => {
		e.setAttribute("src", new URL(e.getAttribute("src"), programXmlFile).toString());
	});
	[].slice.call(contentDiv.getElementsByTagName("webaudio-knob")).map(e => {
		e.setAttribute("src", new URL(e.getAttribute("src"), programXmlFile).toString());
	});
	[].slice.call(contentDiv.getElementsByTagName("webaudio-switch")).map(e => {
		e.setAttribute("src", new URL(e.getAttribute("src"), programXmlFile).toString());
	});
	[].slice.call(contentDiv.getElementsByTagName("webaudio-slider")).map(e => {
		e.setAttribute("src", new URL(e.getAttribute("src"), programXmlFile).toString());
		e.setAttribute("knobsrc", new URL(e.getAttribute("knobsrc"), programXmlFile).toString());
	});

	placeholder.innerHTML = "";
	placeholder.innerHTML = contentDiv.innerHTML;

	setupWebAudioControlEvents();
}

