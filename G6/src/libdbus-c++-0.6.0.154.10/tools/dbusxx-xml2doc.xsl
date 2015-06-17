<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:wy="http://www.wyplay.com/schema"
                version="1.0">

<!-- Apply template for / token -->
<xsl:template match="/">
<html>
<head>
<style type="text/css">
 a:link    { text-decoration:none; font-weight:bold; color:#000000; }
 a:visited { text-decoration:none; font-weight:bold; color:#000000; }
 a:hover   { text-decoration:none; font-weight:bold; background-color:#777777; }
 a:active  { text-decoration:none; font-weight:bold; background-color:#777777; }
</style>
<title>DBus interface definition</title>
</head>
<body>
<xsl:apply-templates select="node"/>
</body>
</html>
</xsl:template>

<!-- Apply template for NODE token -->
<xsl:template match="node">

  <!-- Title -->
  <table bgcolor="#DDDDDD" width="100%" style="border: 1px solid #000000">
  <tr>
  <td align='center' valign='middle'>
  <h1 style="margin-top: 0; margin-bottom: 0;"><xsl:value-of select="@name"/> DBus Interface</h1>
  </td>
  </tr>
  </table>

  <!-- Index table -->
  <h1>Table of contents</h1>
  <table bgcolor="#DDDDDD" width="80%" align="center"><tr><td>
  <ol>
  <a href="#overview"><li/>Global overview</a>
  <xsl:for-each select="interface">
      <a href="{concat('#',@name)}"><li/>Interface <xsl:value-of select="@name"/></a>
      <ol type="a">
      <a href="{concat(concat('#',@name),'.overview')}"><li/>Overview</a>
      <a href="{concat(concat('#',@name),'.identification')}"><li/>Identification</a>
      <a href="{concat(concat('#',@name),'.methods')}"><li/>Methods</a>
      <a href="{concat(concat('#',@name),'.signals')}"><li/>Signals</a>
      </ol>
  </xsl:for-each>
  </ol>
  </td></tr></table>

  <!-- Overview -->
  <ol>
  <a name="overview"><h1><li/> Global overview</h1></a>
  <table align="center" width="80%"><tr><td align="justify">
    <xsl:value-of select="translate(@wy:desc,'[]','&lt;&gt;')" disable-output-escaping="yes"/>
  </td></tr></table>

  <!-- Interfaces -->
  <xsl:apply-templates select="interface"/>
  </ol>

</xsl:template>

<!-- Apply template for INTERFACE token -->
<xsl:template match="interface">

  <!-- Title -->
  <a name="{@name}"><h1><li/> Interface <xsl:value-of select="@name"/></h1></a>

  <!-- Start sublist -->
  <ol type="a">

  <!-- Overview -->
  <a name="{concat(@name,'.overview')}"><h2><li/> Overview</h2></a>
  <table align="center" width="80%"><tr><td align="justify">
    <xsl:value-of select="translate(@wy:desc,'[]','&lt;&gt;')" disable-output-escaping="yes"/>
  </td></tr></table>

  <!-- Identification -->
  <a name="{concat(@name,'.identification')}"><h2><li/> Identification</h2></a>
  <table align="center" width="80%">
  <tr><td width="25%">DBus service:</td><td>
  <xsl:value-of select="substring(translate(/node/@name,'/','.'), 2)"/></td></tr>
  <tr><td>DBus object path:</td><td><xsl:value-of select="/node/@name"/></td></tr>
  <tr><td>DBus interface:</td><td><xsl:value-of select="@name"/></td></tr>
  </table>

  <!-- Handle methods -->
  <a name="{concat(@name,'.methods')}"><h2><li/> Methods</h2></a>
  <xsl:choose>
    <xsl:when test="count(method) > 0"><xsl:apply-templates select="method"/></xsl:when>
    <xsl:otherwise>
      <table align="center" width="80%"><tr><td align="justify">
      No methods are defined in this interface
      </td></tr></table>
    </xsl:otherwise>
  </xsl:choose>

  <!-- Handle signals -->
  <a name="{concat(@name,'.signals')}"><h2><li/> Signals</h2></a>
  <xsl:choose>
    <xsl:when test="count(signal) > 0"><xsl:apply-templates select="signal"/></xsl:when>
    <xsl:otherwise>
      <table width="80%"><tr><td align="justify">
      No signals are defined in this interface
      </td></tr></table>
    </xsl:otherwise>
  </xsl:choose>

  <!-- End sublist -->
  </ol>

</xsl:template>

<!-- Apply template for SIGNAL and METHOD token -->
<xsl:template match="signal|method">
<br/>
<table align="center" bgcolor="#000000" width="80%" cellspacing="1" cellpadding="5">
<tr>
  <td bgcolor="#CCCCCC" width="20%" valign="top">Name</td>
  <td bgcolor="#CCCCCC"><b><xsl:value-of select="@name"/></b></td>
</tr>
<tr>
  <td bgcolor="#FFFFFF" width="20%" valign="top">Signature</td>
  <td bgcolor="#FFFFFF">
    <table cellspacing="0" cellpadding="5" align="left"><xsl:apply-templates select="arg"/></table>
  </td>
</tr>
<tr>
  <td bgcolor="#FFFFFF" width="20%" valign="top">Description</td>
  <td bgcolor="#FFFFFF" align="justify"><xsl:value-of
      select="translate(@wy:desc,'[]','&lt;&gt;')" disable-output-escaping="yes"/></td>
</tr>
</table>
</xsl:template>

<!-- Apply template for ARG token -->
<xsl:template match="arg">
<tr>
  <td valign="top"><xsl:value-of select="@type"/></td>
  <td valign="top">
  <xsl:choose>
    <xsl:when test="@direction = 'out'">[OUT]</xsl:when>
    <xsl:otherwise>[IN]</xsl:otherwise>
  </xsl:choose>
  </td>
  <td width="100%" valign="top">
    <xsl:value-of select="translate(@wy:desc,'[]','&lt;&gt;')" disable-output-escaping="yes"/>
  </td>
</tr>
<xsl:apply-templates/>
</xsl:template>

<!-- Apply template for WY:ALLOWED-VALUES token -->
<xsl:template match="wy:allowed-values">
<tr><td colspan="3">
<table align="center" width="90%" cellspacing="0">
<tr><td bgcolor="#AAAAAA" colspan="2" align="center"><xsl:value-of select="@wy:title"/></td></tr>
<xsl:apply-templates select="wy:value"/>
</table>
</td></tr>
</xsl:template>

<!-- Apply template for WY:VALUE token -->
<xsl:template match="wy:value">
<tr bgcolor="#DDDDDD">
<td align="right" valign="top"><b><xsl:value-of select="@wy:name"/></b></td>
<td><xsl:value-of select="@wy:value"/></td>
</tr>
</xsl:template>

</xsl:stylesheet>
