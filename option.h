#ifndef __CK_OPT_H__
#define __CK_OPT_H__

#include "ckw.h"
#include <string>


class ckOpt {
public:
	ckOpt();
	~ckOpt();

	void		setFile(const char *path=NULL);
	void		loadXdefaults();
	bool		set(int argc, char *argv[]);

	bool		isWinPos()		{ return(m_isWinPos); }
	int		getWinPosX()		{ return(m_winPosX); }
	int		getWinPosY()		{ return(m_winPosY); }
	int		getWinCharW()		{ return(m_winCharW); }
	int		getWinCharH()		{ return(m_winCharH); }
	bool		isIconic()		{ return(m_isIconic); }
	COLORREF	getColorFg()		{ return(m_colorFg); }
	COLORREF	getColorBg()		{ return(m_colorBg); }
	COLORREF	getColorCursor()	{ return(m_colorCursor); }
	COLORREF	getColorCursorIme()	{ return(m_colorCursorIme); }
	COLORREF	getColor(int i)
	{
		return((0 <= i && i <= 15) ? m_colors[i] : m_colors[0]);
	}

	bool		isScrollHide()		{ return(m_scrollHide); }
	bool		isScrollRight()		{ return(m_scrollRight); }
	int		getSaveLines()		{ return(m_saveLines); }
	int		getBorderSize()		{ return(m_borderSize); }
	int		getLineSpace()		{ return(m_lineSpace); }
	int		getTransp()		{ return(m_transp); }
	bool		isTranspColor()		{ return(m_isTranspColor); }
	COLORREF	getTranspColor()	{ return(m_transpColor); }
	bool		isTopMost()		{ return(m_isTopMost); }

	const char*	getCmd()
	{
		return((m_cmd.size()) ? m_cmd.c_str() : NULL);
	}
	int		getFontSize()		{ return(m_fontSize); }
	const char*	getFont()
	{
		return((m_font.size()) ? m_font.c_str() : NULL);
	}
	const char*	getBgBmp()
	{
		return((m_bgBmp.size()) ? m_bgBmp.c_str() : NULL);
	}
	const char*	getCurDir()
	{
		return((m_curDir.size()) ? m_curDir.c_str() : NULL);
	}
	const char*	getTitle()
	{
		return((m_title.size()) ? m_title.c_str() : NULL);
	}


protected:
	void	cmdsMake(int argc, char *argv[]);
	void	geometry(const char *str);
	int	setOption(const char *name, const char *value, bool rsrc);
	void	_loadXdefaults(const char *path);

private:
	bool		m_isWinPos;
	int		m_winPosX;
	int		m_winPosY;
	int		m_winCharW;
	int		m_winCharH;
	bool		m_isIconic;
	std::string	m_cmd;
	std::string	m_font;
	int		m_fontSize;
	COLORREF	m_colorFg;
	COLORREF	m_colorBg;
	COLORREF	m_colorCursor;
	COLORREF	m_colorCursorIme;
	COLORREF	m_colors[17];
	std::string	m_bgBmp;
	bool		m_scrollHide;
	bool		m_scrollRight;
	int		m_saveLines;
	int		m_borderSize;
	int		m_lineSpace;
	int		m_transp;
	bool		m_isTranspColor;
	COLORREF	m_transpColor;
	bool		m_isTopMost;
	std::string	m_curDir;
	std::string	m_title;
	char	m_config_file[MAX_PATH+1];
};

#endif /* __CK_OPT_H__ */
