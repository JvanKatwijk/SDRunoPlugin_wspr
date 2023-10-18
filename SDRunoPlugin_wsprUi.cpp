#include <sstream>
#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/listbox.hpp>
#include <nana/gui/widgets/slider.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/timer.hpp>
#include <unoevent.h>

#include "SDRunoPlugin_wspr.h"
#include "SDRunoPlugin_wsprUi.h"
#include "SDRunoPlugin_wsprForm.h"

// Ui constructor - load the Ui control into a thread
	SDRunoPlugin_wsprUi::
	          SDRunoPlugin_wsprUi (SDRunoPlugin_wspr& parent,
	                               IUnoPluginController& controller) :
	                                          m_parent(parent),
	                                          m_form (nullptr),
	                                          m_controller(controller) {
	m_thread = std::thread(&SDRunoPlugin_wsprUi::ShowUi, this);
}

// Ui destructor (the nana::API::exit_all();) is required if using Nana UI library
	SDRunoPlugin_wsprUi::~SDRunoPlugin_wsprUi() {	
	nana::API::exit_all();
	m_thread.join();	
}

// Show and execute the form
void	SDRunoPlugin_wsprUi::ShowUi () {	
	m_lock. lock ();
	m_form = std::make_shared<SDRunoPlugin_wsprForm>(*this, m_controller);
	m_lock. unlock ();

	m_form->Run();
}

// Load X from the ini file (if exists)
// TODO: Change test to plugin name
int	SDRunoPlugin_wsprUi::LoadX () {
	std::string tmp;
	m_controller.GetConfigurationKey ("wspr.X", tmp);
	if (tmp.empty()) {
	   return -1;
	}
	return stoi(tmp);
}

// Load Y from the ini file (if exists)
// TODO: Change test to plugin name
int	SDRunoPlugin_wsprUi::LoadY () {
	std::string tmp;
	m_controller.GetConfigurationKey ("wspr.Y", tmp);
	if (tmp.empty()) {
	   return -1;
	}
	return stoi(tmp);
}

// Handle events from SDRuno
// TODO: code what to do when receiving relevant events
void	SDRunoPlugin_wsprUi::HandleEvent (const UnoEvent& ev) {
	switch (ev. GetType ()) {
	   case UnoEvent::StreamingStarted:
	      break;

	   case UnoEvent::StreamingStopped:
	      break;

	   case UnoEvent::SavingWorkspace:
	      break;

	   case UnoEvent::ClosingDown:
	      FormClosed ();
	      break;

	   default:
	      break;
	}
}

// Required to make sure the plugin is correctly unloaded when closed
void	SDRunoPlugin_wsprUi::FormClosed() {
	m_controller. RequestUnload (&m_parent);
}

void	SDRunoPlugin_wsprUi::set_band	(const std::string &s) {
	m_controller. SetConfigurationKey ("wspr.lastBand", s);
	m_parent. set_band (s);
}

void	SDRunoPlugin_wsprUi::set_callSign	() {
	m_parent. set_callSign ();
}

std::string SDRunoPlugin_wsprUi::load_callSign () {
std::string callsign;
	m_controller. GetConfigurationKey ("wspr.callsign", callsign);
	if (callsign. empty ())
	   return "";
	if (callsign. length () < 3)
	   return "";
	return callsign;
}

void	SDRunoPlugin_wsprUi::store_callSign (const std::string &s) {
	if (s. empty ())
	   return;

	m_controller. SetConfigurationKey ("wspr.callsign", s);
}

std::string SDRunoPlugin_wsprUi::load_grid () {
std::string grid;
	m_controller. GetConfigurationKey ("wspr.grid", grid);
	if (grid. empty ())
	   return "";
	return grid;
}

void	SDRunoPlugin_wsprUi::store_grid (const std::string &s) {
	if (s. empty ())
	   return;

	m_controller. SetConfigurationKey ("wspr.grid", s);
}

std::string SDRunoPlugin_wsprUi::load_antenna () {
std::string antenna;
	m_controller. GetConfigurationKey ("wspr.antenna", antenna);
	if (antenna. empty ())
	   return "";
	return antenna;
}

void	SDRunoPlugin_wsprUi::store_antenna (const std::string &s) {
	if (s. empty ())
	   return;

	m_controller. SetConfigurationKey ("wspr.antenna", s);
}

void	SDRunoPlugin_wsprUi::show_status	(const std::string &s) {
	std::lock_guard<std::mutex> l (m_lock);
	if (m_form != nullptr)
	   m_form -> show_status (s);
}

void	SDRunoPlugin_wsprUi::show_results	(const std::string & s) {
	std::lock_guard<std::mutex> l (m_lock);
	if (m_form != nullptr)
	   m_form -> show_results (s);
}

void	SDRunoPlugin_wsprUi::addMessage	(const std::string &s) {
	std::lock_guard<std::mutex> l (m_lock);
	if (m_form != nullptr)
	   m_form -> addMessage (s);
}

void	SDRunoPlugin_wsprUi::set_subtraction (bool b) {
	m_parent. set_subtraction (b);
}

void	SDRunoPlugin_wsprUi::set_quickMode   (bool b) {
	m_parent. set_quickMode (b);
}

void	SDRunoPlugin_wsprUi::switch_reportMode   () {
	m_parent. switch_reportMode ();
}

void	SDRunoPlugin_wsprUi::show_reportMode	(bool b) {
	std::lock_guard<std::mutex> l (m_lock);
	if (m_form != nullptr)
	   m_form -> show_reportMode (b);
}
	
void	SDRunoPlugin_wsprUi::handle_reset	() {
	m_parent. handle_reset ();
}

void	SDRunoPlugin_wsprUi::display_callsign	(const std::string &s) {
	std::lock_guard<std::mutex> l (m_lock);
	if (m_form != nullptr)
	   m_form -> display_callsign (s);
}


void	SDRunoPlugin_wsprUi::display_grid	(const std::string &s) {
	std::lock_guard<std::mutex> l (m_lock);
	if (m_form != nullptr)
	   m_form -> display_grid (s);
}

void	SDRunoPlugin_wsprUi::show_printStatus	(const std::string &s) {
	std::lock_guard<std::mutex> l (m_lock);
	if (m_form != nullptr)
	   m_form -> show_printStatus (s);
}

void	SDRunoPlugin_wsprUi::show_version	(const std::string &s) {
	std::lock_guard<std::mutex> l (m_lock);
	if (m_form != nullptr)
	   m_form -> show_version (s);
}

bool	SDRunoPlugin_wsprUi::set_wsprDump	() {
	return m_parent. set_wsprDump	();
}

void	SDRunoPlugin_wsprUi::initBand	() {
std::string band;
	m_controller.GetConfigurationKey ("wspr.lastBand", band);
	std::lock_guard<std::mutex> l (m_lock);
	if (m_form != nullptr)
		if (band != "")
	       m_form -> initBand (band);
	    else
	       m_form -> initBand ("80m");
}

std::string	SDRunoPlugin_wsprUi::get_LastBand () {
std::string band = "";
	m_controller.GetConfigurationKey ("wspr.lastBand", band);
	if (band == "")
	   return "20m";
	return band;
}

	
