<xsl:transform version="1.0"
	xmlns:svg="http://www.w3.org/2000/svg"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="html" indent="yes" />
  <xsl:preserve-space elements="script webaudio-knob webaudio-switch" />

  <xsl:template match="GUI">
    <html>
		<meta charset="utf-8" />
	  <head>
		  <script src="../../../webcomponents-lite.js">&#10;</script>
		  <script src="../../../webaudio-controls.js">&#10;</script>
	  </head>
      <body>
		<div width="{@w}px" height="{@h}px">
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
	<webaudio-knob id="knob-{@param}"
		src="{@image}" style="position:absolute; left:{@x}px; top:{@y}px" 
		value="0" max="{@frames}" step="1" sprites="{@frames - 1}">
	&#10;
	</webaudio-knob>
  </xsl:template>

  <xsl:template match="Slider">
	<webaudio-slider id="knob-{@param}"
		src="{@image_bg}" knobsrc="{@image_handle}" style="position:absolute; left:{@x}px; top:{@y}px" 
		value="0" max="{@frames}" step="1">
		<xsl:attribute name="direction">
			<xsl:choose>
				<xsl:when test="@orientation='vertical'">vert</xsl:when>
				<xsl:otherwise>horz</xsl:otherwise>
			</xsl:choose>
		</xsl:attribute>
	&#10;
	</webaudio-slider>
  </xsl:template>

  <xsl:template match="OnOffButton">
	<webaudio-switch id="switch-{@param}"
		src="{@image}" style="position:absolute; left:{@x}px; top:{@y}px">
	&#10;
	</webaudio-switch>
  </xsl:template>

</xsl:transform>

