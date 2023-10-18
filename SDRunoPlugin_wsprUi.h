#pragma once

#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/timer.hpp>
#include <iunoplugin.h>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <iunoplugincontroller.h>
#include "SDRunoPlugin_wsprForm.h"

// Forward reference
class SDRunoPlugin_wspr;

class SDRunoPlugin_wsprUi {
public:

		SDRunoPlugin_wsprUi	(SDRunoPlugin_wspr& parent,
	                                 IUnoPluginController& controller);
		~SDRunoPlugin_wsprUi	();

	void	HandleEvent		(const UnoEvent& evt);
	void	FormClosed		();
	void	ShowUi			();

	int	LoadX			();
	int	LoadY			();

	void	initBand		();
	std::string get_LastBand	();

	void	set_band	(const std::string &);
	void	set_callSign	();

	void	show_status	(const std::string &);
	void	show_results	(const std::string &);

	void	store_callSign	(const std::string &);
	void	store_grid	(const std::string &);
	void	store_antenna	(const std::string &);
	std::string load_callSign	();
	std::string load_grid	();
	std::string load_antenna	();
	void	addMessage	(const std::string &);

	void	set_subtraction	(bool);
	void	set_quickMode	(bool);
	void	switch_reportMode	();

	void	handle_reset	();
	void	show_printStatus	(const std::string &);
	void	show_version		(const std::string &);
	void	display_callsign	(const std::string &);
	void	display_grid		(const std::string &);
	void	show_reportMode		(bool);

	bool	set_wsprDump		();
private:
	
	SDRunoPlugin_wspr & m_parent;
	std::thread m_thread;
	std::shared_ptr<SDRunoPlugin_wsprForm> m_form;
	bool m_started;
	std::mutex m_lock;
	IUnoPluginController & m_controller;
};
