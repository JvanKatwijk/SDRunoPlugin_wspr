#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <iunoplugincontroller.h>
#include <iunoplugin.h>
#include <iunostreamobserver.h>
#include <iunoaudioobserver.h>
#include <iunoaudioprocessor.h>
#include <iunostreamobserver.h>
#include <iunoannotator.h>
#include "SDRunoPlugin_wsprUi.h"
#include	<queue>
#include	"constants.h"
#include        "ringbuffer.h"
#include        "decimator.h"
#include        ".\wsprd\wsprd.h"
#include	"./fft/fft-handler.h"

#include	<complex>

class	reporterWriter;

class SDRunoPlugin_wspr : public IUnoPlugin,
	                          public IUnoStreamProcessor,
                                  public IUnoAudioProcessor {

public:
	
		SDRunoPlugin_wspr (IUnoPluginController& controller);
virtual ~SDRunoPlugin_wspr ();

	virtual const
	char*	GetPluginName() const override { return "wspr decoder"; }

	void	StreamProcessorProcess (channel_t channel,
	                                Complex *buffer, int length,
	                                    bool& modified) override;
        void    AudioProcessorProcess (channel_t channel,
                                       float *buffer,
                                       int length, bool& modified) override;

	// IUnoPlugin
	virtual
	void HandleEvent		(const UnoEvent& ev) override;

	void		set_band	(const std::string &);
	void		set_callSign	();

	void		set_subtraction	(bool);
	void		set_quickMode	(bool);
	void		switch_reportMode	();

	void		handle_reset	();

	bool		set_wsprDump	();
private:
	void		printSpots	(std::vector<decoder_results> &,
	                                             struct decoder_options &);
	void		postSpots	(std::vector<decoder_results> &);

//	std::queue<int>	frequencyQueue;
	std::vector<std::complex<float>> tones;
	int		tonePhase;
	int		toneFreq;
	void		honor_freqRequest	();
	IUnoPluginController *m_controller;
	void		workerFunction	();
	std::mutex	m_lock;
	std::thread	* m_worker;
	SDRunoPlugin_wsprUi m_form;
	decimator	decimator_0;
	decimator	decimator_1;
	decimator	decimator_2;
	RingBuffer<std::complex<float>> inputBuffer;
	struct	decoder_options	dec_options;
	struct	decoder_results	dec_results [50];
	std::queue<struct decoder_options> optionsQueue;

	reporterWriter	*theWriter;
	FILE		*filePointer;
	struct plugin_settings {
	   uint32_t	dialFreq;
	   uint32_t	realFreq;
	   uint32_t	maxloop;
	   uint32_t	nloop;
	   bool		report;
	} rx_options;

	struct plugin_state {
	   std::atomic<bool>	frequencyChange;
	   std::atomic<bool>	running;
	   std::atomic<bool>	savingSamples;
	   std::atomic<bool>	resetRequest;
	   struct tm	*gtm;
	} rx_state;

	fftHandler	fft;

	std::complex<float> buffer [SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE];
	float		samples_i [SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE];
	float		samples_q [SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE];
	int		newFrequency;
	typedef struct {
		time_t time;
		struct decoder_results callRecord;
	} listElement;

	std::list<listElement> resultQueue;
	bool		recentlySeen (struct decoder_results &lastRes,
                                         time_t time);

	std::mutex	printing;
	void		processBuffer	(std::complex<float> *, 
	                                 struct decoder_options &);
};

