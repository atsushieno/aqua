<xsl:transform version="1.0"
     xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
     xmlns:a="urn:aria2svg/ns/1.0"
     xmlns:svg="http://www.w3.org/2000/svg">
  <xsl:output method="xml" indent="yes" />
  <xsl:namespace-alias stylesheet-prefix="svg" result-prefix="svg" />

  <xsl:template match="GUI">
    <svg:svg width="{@w}pt" height="{@h}pt">
	  <svg:script language="text/javascript"><![CDATA[
		  aria2SvgNS = "urn:aria2svg/ns/1.0";
		  svgNS = "http://www.w3.org/2000/svg";
		  
		  function registerKnob(evt) {
			evt.target.addEventListener("wheel", onKnobWheel);
		  }
		  
		  function onKnobWheel(evt) {
		    var el = this;
			var para = el.getAttributeNodeNS(aria2SvgNS, "param");
			var current = Number(el.getAttributeNS(aria2SvgNS, "current"));
			var deltaYMod = typeof(evt.wheelDelta) == "undefined" ? (-evt.detail / 3) :
				evt.deltaY > 0 ? -1 : 1; // wheel up: +1 / wheel down: -1
			var delta = deltaYMod * Math.round(evt.wheelDeltaY / evt.wheelDelta);
			current += delta;
			console.log("knob: " + para.nodeValue + " = " + current);
			el.setAttributeNS(aria2SvgNS, "current", current);
		  }
	  //]]></svg:script>
      <svg:defs>
        <svg:svg viewBox="0 0 30 32" id="on-off-button-template">
          <!-- svg:rect x="4pt" y="4pt" width="22pt" height="22pt" style="fill: black" />
          <svg:rect x="6pt" y="6pt" width="18pt" height="18pt" style="fill: blue" / -->
        </svg:svg>
      </svg:defs>
      <xsl:apply-templates />
    </svg:svg>
  </xsl:template>

  <xsl:template match="StaticImage">
    <svg:image x="{@x}pt" y="{@y}pt" width="{@w}pt" height="{@h}pt" href="{@image}" class="StaticImage" />
  </xsl:template>

  <xsl:template match="Knob">
	<svg:svg x="{@x}pt" y="{@y}pt" width="36pt" height="36pt" a:param="{@param}" a:current="0" a:frames="{@frames}" onload="registerKnob(evt)" class="Knob">
      <svg:image href="{@image}" class="knob-image" />
	</svg:svg>
  </xsl:template>

  <xsl:template match="OnOffButton">
    <svg:svg x="{@x}pt" y="{@y}pt" width="{@w}pt" height="{@h}pt" class="OnOffButton">
      <svg:use href="#on-off-button-template" a:current="0" />
      <svg:image href="{@image}" />
    </svg:svg>
  </xsl:template>

</xsl:transform>

