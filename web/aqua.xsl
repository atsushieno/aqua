<xsl:transform version="1.0"
	xmlns:svg="http://www.w3.org/2000/svg"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="xml" indent="yes" />
  <xsl:variable name="InstrumentProgramId" select="'(specify-identifier)'" />

  <xsl:template match="GUI">
    <html>
		<meta charset="utf-8" />
		<meta http-equiv="Cache-Control" content="no-cache" />
		<meta http-equiv="Cache-Control" content="no-store" />
	  <head>
		  <script src="../../../webcomponents-lite.js"><xsl:text> </xsl:text></script>
		  <script src="../../../webaudio-controls.js"><xsl:text> </xsl:text></script>
		  <script src="../../../aqua.js"><xsl:text> </xsl:text></script>
		  <script type="text/javascript"><xsl:comment>
		  document.AquaInstrument = "<xsl:value-of select="$InstrumentProgramId" />";
		  </xsl:comment></script>
	  </head>
      <body onload="setupWebAudioControlEvents()">
		<div style="width:{@w}px; height:{@h}px">
			<xsl:apply-templates />
		</div>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="StaticText">
    <!-- FIXME: there is `transparent` attribute too -->
	<div style="position:absolute; left:{@x}px; top:{@y}px; width:{@w}px; height:{@h}px">
	    <svg:svg viewBox="0px 0px {@w}px {@h}px"><svg:text x="0px" y="{@h}px" style="color:{@color_text}"> <xsl:value-of select="@text" /></svg:text></svg:svg>
	</div>
	<!-- span style="position:absolute; text-align:justify; left:{@x}px; top:{@y}px; width:{@w}px; height:{@h}px; color:{@color_text}" class="StaticText"><xsl:value-of select="@text" /></span -->
  </xsl:template>

  <xsl:template match="StaticImage">
    <img src="{@image}" style="position:absolute; left:{@x}px; top:{@y}px; width:{@w}px; height:{@h}px" class="StaticImage" />
  </xsl:template>

  <xsl:template match="Knob">
	<webaudio-knob id="knob-{@param}" a2w-control="{@param}"
		src="{@image}" style="position:absolute; left:{@x}px; top:{@y}px" 
		value="0" max="{@frames - 1}" step="1" sprites="{@frames - 1}" tooltip="%d">
	<xsl:text> </xsl:text>
	</webaudio-knob>
  </xsl:template>

  <xsl:template match="Slider">
	<webaudio-slider id="knob-{@param}" a2w-control="{@param}"
		src="{@image_bg}" knobsrc="{@image_handle}" style="position:absolute; left:{@x}px; top:{@y}px" 
		value="0" max="100" step="1" diameter="-1" tooltip="%d">
		<xsl:attribute name="direction">
			<xsl:choose>
				<xsl:when test="@orientation='vertical'">vert</xsl:when>
				<xsl:otherwise>horz</xsl:otherwise>
			</xsl:choose>
		</xsl:attribute>
	<xsl:text> </xsl:text>
	</webaudio-slider>
  </xsl:template>

  <xsl:template match="OnOffButton">
	<webaudio-switch id="switch-{@param}" a2w-control="{@param}"
		src="{@image}" style="position:absolute; left:{@x}px; top:{@y}px" diameter="-1">
	<xsl:text> </xsl:text>
	</webaudio-switch>
  </xsl:template>

  <xsl:template match="*">
	<xsl:message>Unknown element: <xsl:copy-of select="name()" /></xsl:message>
  </xsl:template>

</xsl:transform>

