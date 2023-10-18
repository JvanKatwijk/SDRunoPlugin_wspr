#pragma once

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/listbox.hpp>
#include <nana/gui/widgets/slider.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/combox.hpp>
#include <nana/gui/timer.hpp>
#include <nana/gui/widgets/picture.hpp>
#include <nana/gui/filebox.hpp>
#include <nana/gui/dragger.hpp>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <iunoplugincontroller.h>

// Shouldn't need to change these
#define topBarHeight (27)
#define bottomBarHeight (8)
#define sideBorderWidth (8)

// TODO: Change these numbers to the height and width of your form
#define formWidth (580)
#define formHeight (540)

class SDRunoPlugin_wsprUi;

class SDRunoPlugin_wsprForm : public nana::form {

public:

	SDRunoPlugin_wsprForm (SDRunoPlugin_wsprUi& parent,
	                       IUnoPluginController& controller);		
	~SDRunoPlugin_wsprForm();
	
	void	Run		();
	void	set_band	(const std::string &);
	void	set_callSign	();
	void	show_status	(const std::string &);
	void	show_results	(const std::string &);
	void	addMessage	(const std::string &);

	void	initBand	(const std::string &);
	void	set_subtraction	();
	void	set_quickMode	();
	void	switch_reportMode	();

	void	handle_reset	();
	void	show_printStatus	(const std::string &);
	void	show_version		(const std::string& );
	void	display_callsign	(const std::string &);
	void	display_grid		(const std::string &);
	void	show_reportMode		(bool);

	bool	set_wsprDump		();
private:

	void Setup();

	// The following is to set up the panel graphic to look like a standard SDRuno panel
	nana::picture bg_border{ *this, nana::rectangle(0, 0, formWidth, formHeight) };
	nana::picture bg_inner{ bg_border, nana::rectangle(sideBorderWidth, topBarHeight, formWidth - (2 * sideBorderWidth), formHeight - topBarHeight - bottomBarHeight) };
	nana::picture header_bar{ *this, true };
	nana::label title_bar_label{ *this, true };
	nana::dragger form_dragger;
	nana::label form_drag_label{ *this, nana::rectangle(0, 0, formWidth, formHeight) };
	nana::paint::image img_min_normal;
	nana::paint::image img_min_down;
	nana::paint::image img_close_normal;
	nana::paint::image img_close_down;
	nana::paint::image img_header;
	nana::picture close_button{ *this, nana::rectangle(0, 0, 20, 15) };
	nana::picture min_button{ *this, nana::rectangle(0, 0, 20, 15) };

	// Uncomment the following 4 lines if you want a SETT button and panel
	nana::paint::image img_sett_normal;
	nana::paint::image img_sett_down;
	nana::picture sett_button{ *this, nana::rectangle(0, 0, 40, 15) };
	void SettingsButton_Click();

	// TODO: Now add your UI controls here

	nana::combox bandSelector {*this,
                                    nana::rectangle ( 30, 30, 80, 20)};
	nana::button callsign_Button {*this,
                                    nana::rectangle ( 120, 30, 40, 20)};
	nana::label statusLine	  {*this,
                                    nana::rectangle ( 170, 30, 120, 20)};
	nana::label resultLine	  {*this,
                                    nana::rectangle ( 300, 30, 80, 20)};
	nana::label printStatus	  {*this,
	                            nana::rectangle ( 390, 30, 90, 20)};
	nana::button dumpButton   {*this,
	                            nana::rectangle ( 490, 30, 50, 20)};
	nana::button subtractionButton {*this,
                                    nana::rectangle ( 30, 60, 60, 20)};
	nana::button quickModeButton {*this,
                                    nana::rectangle ( 100, 60, 60, 20)};
	nana::button reportModeButton {*this,
                                    nana::rectangle ( 170, 60, 80, 20)};
	nana::button resetButton   {*this,
	                            nana::rectangle ( 260, 60, 60, 20)};
	
	nana::label homeCall	   {*this,
	                            nana::rectangle ( 330, 60, 60, 20)};
	nana::label homeLoc	   {*this,
	                            nana::rectangle (400, 60, 60,  20)};
	nana::label copyRightLabel {*this,
		                        nana::rectangle (470, 60, 20, 20)};
	nana::label  versionLabel {*this,
	                                nana::rectangle (490, 60, 40, 20)};

	std::list<std::string> displayList;
	nana::label textBlock	  {*this,
                                    nana::rectangle ( 30, 90, 520, 400)};
	SDRunoPlugin_wsprUi & m_parent;
	IUnoPluginController & m_controller;
};
