import sys
import re
from xml.etree import ElementTree

def importGui(guiXmlFileName):
    exec("xsltproc", "aria2web.xsl", guiXmlFileName)
    

if len(sys.argv) <= 1:
    print("USAGE: import-aria [path/to/bank-xml]")
    sys.exit(1)

# ARIA extensions XML-ish content is annoying - it is neither a well-formed single-rooted document nor a valid external entity that can be parsed as an XML "document fragment" (its **Text** declaration lacks mandatory encoding declaration).
# We have an ugly hack here, to remove **XML** declaration and combine with a dummy root element so that it can be read as a well-formed document entity.
bankXmlFragmentReader = open(sys.argv[1], "r")
bankXmlFragmentTextRaw = bankXmlFragmentReader.read()
pattern = re.compile("<\?xml[\s]+version[\s]*=\\\"1.0\\\"[\s]*\?>")
bankXmlFragmentText = pattern.sub('', bankXmlFragmentTextRaw)

ariaInfoTree = ElementTree.fromstring("<dummy>" + bankXmlFragmentText + "</dummy>")

bank = ariaInfoTree.find("AriaBank")
ariaGUIElement = bank.find("AriaGUI")
ariaPrograms = bank.findall("AriaProgram")

for ariaProgram in ariaPrograms:
    print("Importing " + ariaProgram.attrib("name"))
    importGui(ariaProgram.attrib("gui"))

