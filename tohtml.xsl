<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:template match="/">
    <html>
      <body>
        <table border="1">
          <tr>
            <th align="left">Testcase</th>
            <th align="left">Result</th>
            <th>Failure</th>
          </tr>
          <xsl:for-each select="ciut/test">
            <xsl:sort select="@name"/>
            <tr>
              <xsl:choose>
                <xsl:when test="@result=&quot;OK&quot;">
                  <td bgcolor="#00ff00"><xsl:value-of select="@name"/></td>
                  <td bgcolor="#00ff00"><xsl:value-of select="@result"/></td>
                </xsl:when>
                <xsl:otherwise>
                  <td bgcolor="#ff0000"><xsl:value-of select="@name"/></td>
                  <td bgcolor="#ff0000"><xsl:value-of select="@result"/></td>
                  <td><pre><xsl:value-of select="./log/failure"/></pre></td>
                </xsl:otherwise>
              </xsl:choose>
            </tr>
          </xsl:for-each>
        </table>
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>
