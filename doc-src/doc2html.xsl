<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:template match="doc">
    <html>
    <meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1"/>
    <title><xsl:value-of select="@title"/></title>
    <body>
    <H3><xsl:value-of select="@title"/></H3>
    <xsl:apply-templates select="abstract"/>
    <ul><xsl:apply-templates select="chapter" mode="toc"/></ul>
    <xsl:for-each select="chapter">
      <a name="{generate-id()}"><H4><xsl:value-of select="@title"/></H4></a>
      <xsl:apply-templates/>
    </xsl:for-each>
    </body>
    </html>
  </xsl:template>

  <xsl:template match="abstract">
    <table><tr><td width="10%"/><td width="80%"><xsl:apply-templates/></td><td width="10%"/></tr></table>
  </xsl:template>

  <xsl:template match="section">
    <a name="{@title}"><H5><xsl:value-of select="@title"/></H5></a>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="chapter" mode="toc">
    <li><a href="#{generate-id()}"><xsl:value-of select="@title"/></a></li>
  </xsl:template>

  <xsl:template match="//definitions">
    <table>
    <th align="left"><xsl:value-of select="@notion"/></th>
    <th width="10"></th>
    <th align="left"><xsl:value-of select="@meaning"/></th>
    <xsl:for-each select="def">
      <xsl:sort select="@notion"/>
      <tr><td valign="top"><xsl:value-of select="@notion"/></td><td></td>
          <td valign="top"><xsl:apply-templates/></td></tr>
    </xsl:for-each>
    </table><br/>
  </xsl:template>

  <xsl:template match="p">
    <p/>
  </xsl:template>

  <xsl:template match="id">
    <span style="color:7f7f00"><xsl:value-of select="."/></span>
  </xsl:template>

  <xsl:template match="code">
    <pre style="color:7f7f00"><xsl:value-of select="."/></pre>
  </xsl:template>

  <xsl:template match="list">
    <ul><xsl:apply-templates/></ul>
  </xsl:template>
  <xsl:template match="li">
    <li><xsl:apply-templates/></li>
  </xsl:template>

  <xsl:template match="link">
    <xsl:for-each select="@section">
      <A href="#{.}"><xsl:value-of select="."/></A>
    </xsl:for-each>
    <xsl:for-each select="@url">
      <A href="{.}"><xsl:value-of select=".."/></A>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>
