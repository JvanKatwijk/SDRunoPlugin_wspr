#include	<sstream>
#include	<unoevent.h>
#include	<iunoplugincontroller.h>
#include	<vector>
#include	<list>
#include	<sstream>
#include	<chrono>
#include	<Windows.h>
#include	"SDRunoPlugin_wspr.h"
#include	"SDRunoPlugin_wsprUi.h"
#include        <complex>
#include        <atomic>
//#include	".\curl/curl.h"

#include	"psk-reporter.h"

const char wsprnet_app_version[]  = "SDRuno-wspr";  // 10 chars max.!
const	char *Version	= "0.7";

static
int	gettimeofday (struct timeval* tv, struct timezone* tz) {
static LONGLONG birthunixhnsec = 116444736000000000;  /*in units of 100 ns */
FILETIME systemtime;

	GetSystemTimeAsFileTime (&systemtime);
	ULARGE_INTEGER utime;
	utime.LowPart  = systemtime. dwLowDateTime;
	utime.HighPart = systemtime. dwHighDateTime;

	ULARGE_INTEGER birthunix;
	birthunix. LowPart  = (DWORD)birthunixhnsec;
	birthunix. HighPart = birthunixhnsec >> 32;

	LONGLONG usecs;
	usecs = (LONGLONG)((utime.QuadPart - birthunix.QuadPart) / 10);
	tv -> tv_sec  = (long)(usecs / 1000000);
	tv -> tv_usec = (long)(usecs % 1000000);
	return 0;
}

std::string getTime () {
time_t localtime;
struct tm       *gtm;

	time (&localtime);
	gtm = gmtime (&localtime);
	return std::to_string (gtm -> tm_hour) + "-" +
	              std::to_string (gtm -> tm_min);
}
//
//	The incoming signal has a rate of 192000, in step 1 we decimate
//	to 6000, in step 2 to 375
	SDRunoPlugin_wspr::
	           SDRunoPlugin_wspr (IUnoPluginController& controller):
	                              IUnoPlugin (controller),
	                              m_form	(*this, controller),
	                              m_worker	(nullptr),
	                              decimator_0 (21, 1000, 
	                                           SAMPLING_RATE, 16),
	                              decimator_1 (21, 1000, 12000, 4),
	                              decimator_2 (21, 150, 3000, 8),
	                              inputBuffer (32 * 32768),
	                              fft (512, false) {
	m_controller	= &controller;
   
	memset (dec_options. rcall, 0, 13);
	memset (dec_options. rloc, 0, 7);
//
//	default frequency is in the 20m band
	rx_state.	frequencyChange. store (false);
	rx_state.	running. store (false);
	rx_state.	savingSamples. store (false);
	rx_options.	dialFreq	= 14095600;
	rx_options.	realFreq	= rx_options. dialFreq;
	rx_options.	report		= false;
	theWriter			= nullptr;
	dec_options.	freq		= rx_options. dialFreq;
	dec_options.	usehashtable	= false;
	dec_options.	npasses		= 2;
	dec_options.	subtraction	= true;
	dec_options.	quickmode	= false;
	m_controller	-> SetVfoFrequency (0, (float)dec_options. freq);
	m_controller	-> SetCenterFrequency (0,
	                                      (float)dec_options. freq + 1500);

	m_form. show_version (Version);
//
//	note that the dec_options. rcall and rloc are set to zero already
	std::string cs	= m_form. load_callSign ();
	std::string gr	= m_form. load_grid ();
	for (int i = 0; i < cs. size (); i ++)
	   dec_options. rcall [i] = cs. c_str () [i];
	for (int i = 0; i < gr. size (); i ++)
	   dec_options. rloc [i] = gr. c_str () [i];

	if (cs == "") 
	   m_form. display_callsign ("No call");
	else
	   m_form. display_callsign (cs);

	if (gr == "")
	   m_form. display_grid ("No grid");
	else
	   m_form. display_grid (gr);

	filePointer	= nullptr;
	theWriter = nullptr;
	m_controller	-> SetDemodulatorType (0,
	                             IUnoPluginController::DemodulatorIQOUT);
	m_controller	-> RegisterAudioProcessor(0, this);
	m_worker        =
	        new std::thread (&SDRunoPlugin_wspr::workerFunction, this);
}

	SDRunoPlugin_wspr::~SDRunoPlugin_wspr () {
	rx_state. running. store (false);
	m_form.	show_results ("going to stop"); 
	m_worker	-> join ();
	m_controller    -> UnregisterAudioProcessor (0, this);
	if (filePointer != nullptr)
	   fclose (filePointer);
	if (theWriter != nullptr)
		delete theWriter;
}

void	SDRunoPlugin_wspr::HandleEvent (const UnoEvent& ev) {
	switch (ev. GetType ()) {
	   case UnoEvent::FrequencyChanged:
	      break;

	   case UnoEvent::CenterFrequencyChanged:
	      break;

	   default:
	      m_form. HandleEvent (ev);
	      break;
	}
}

void    SDRunoPlugin_wspr::StreamProcessorProcess (channel_t channel,
	                                           Complex *buffer,
	                                           int length,
	                                           bool& modified) { 
	(void)channel; (void)buffer; (void)length;
//	modified = false;
}
//
//	we get the samples "in" with a specified rate of 192000,
//	we decimate in 2 steps to 375 Ss and store SIGNAL_LENGTH seconds of
//	data in the buffer
void    SDRunoPlugin_wspr::AudioProcessorProcess (channel_t channel,
	                                          float* buffer,
	                                          int length,
	                                          bool& modified) {
static	int teller = 0;
//	Handling IQ input, note that SDRuno interchanges I and Q elements

	if (!modified) {
	   for (int i = 0; i < length; i++) {
	      std::complex<float> sample =
	                   std::complex<float>(buffer [2 * i + 1],
	                                       buffer [2 * i]);

	      if (decimator_0. Pass (sample, &sample) &&
	          decimator_1. Pass (sample, &sample) &&
	          decimator_2. Pass (sample, &sample)) {
	         if (rx_state. savingSamples. load ()) {
	            inputBuffer. putDataIntoBuffer (&sample, 1);
	            teller ++;
	            if (teller % SIGNAL_SAMPLE_RATE == 0) 
	               m_form.show_status ("reading  " +
	                     std::to_string (teller / SIGNAL_SAMPLE_RATE));
			
	            if (inputBuffer. GetRingBufferReadAvailable () >=
	                               SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE) {
	               teller = 0;
	               rx_state. savingSamples.store (false);
	            }
	         }
	      }
	   }
	}
}
//
//	Data collection takes place in a function, called from
//	within amother thread. We create a worker thread that
//	will monitor the time, and  - if the time is right -
//	signal the data reader to collect samples for 116 seconds
//	The worker will collect the samples, most likely within
//	4 seconds, and then signal the reader that a new collecting phase
//	mau start. While the reader is collecting the data, the
//	worker will process the data

void	SDRunoPlugin_wspr::wait_to_start () {
struct timeval lTime;
//	on start up,  Wait for timing alignment, here we
//	compute the delay
	gettimeofday (&lTime, nullptr);
	uint32_t sec   = lTime. tv_sec % 120;
	uint32_t usec  = sec * 1000000 + lTime. tv_usec;
//
//      waiting is for an even time in minutes, i.e. 120 seconds
	uint32_t uwait	=  120000000 - usec - 10000;
	while (rx_state. running. load () && (uwait / 1000000 > 1)) {
	   if (rx_state. frequencyChange. load ()) {
	      rx_options. dialFreq      = newFrequency;
	      dec_options. freq         = newFrequency;
	      m_controller		-> SetVfoFrequency (0,
	                                              (float)newFrequency);
	      m_controller		-> SetCenterFrequency (0,
	                                        (float)newFrequency + 1500);
	      rx_state. frequencyChange. store (false);
//	we cannot assume that the above statements do not take
//	time, recompute the delay
	      gettimeofday (&lTime, nullptr);
	      uint32_t sec = lTime.tv_sec % 120;
	      uint32_t usec = sec * 1000000 + lTime.tv_usec;
	      uwait = 120000000 - usec - 10000;
	   }
	   m_form. show_status ("waiting  " + std::to_string (uwait / 1000000));
	   Sleep (1000); // sleep in milliseconds, so, wait 1 second
	   uwait -= 1000000;
	}
	if (rx_state. running. load () && (uwait > 0))
	   Sleep (uwait / 1000); 	// Sleep in milliseconds
}

static
std::complex<float> buffer [SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE];
void	SDRunoPlugin_wspr::workerFunction() {
	int	cycleCounter = 1;
	rx_state.running.store(true);
	m_form. show_results ("running");
	//
	//	at first we wait until we have an "even" second
	wait_to_start();
	if (!rx_state.running.load())
		return;
	//	then we signal that we want samples to be read in
	rx_state.savingSamples.store(true);
	//
	//	and of course, we have to wait now
	while (rx_state.running.load()) {
		m_form.show_status("reader is working");
		while (rx_state.running.load() &&
			(inputBuffer.GetRingBufferReadAvailable() <
				SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE)) {
			Sleep(100);	// here: milli seconds
		}
		if (!rx_state.running.load())
			break;

		//	   The "reader" probably has already set the flag to false
		//	   but we'll do it anyway
		rx_state.savingSamples.store(false);
		//
		//	   read the buffer, length is known
		int N = inputBuffer.
			getDataFromBuffer(buffer, SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE);
		if (N < SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE) 
			return;		// should not happen
			//
			inputBuffer.FlushRingBuffer();
			//	This seems the right moment to honor switchof frequency
			wait_to_start();
			if (!rx_state.running.load())
				break;
			rx_state.savingSamples.store(true);
			//
			//	and, while the reader callback is reading in the samples for the
			//	next 114 to 117 seconds and putting them into the buffer,
			//	there is ample time to process the data of the previous cycle
			m_form.show_results("buffer " +
				std::to_string(cycleCounter));
			processBuffer(buffer);
			cycleCounter++;
		}
	}

//	filling the buffer takes about 116 seconds, during that
//	period we process the previously filled buffer
void	SDRunoPlugin_wspr::processBuffer (std::complex<float> *b) {
//	Normalize the sample @-3dB 
	float maxSig = 1e-24f;
	for (int i = 0; i < SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE; i ++) {
	   if (abs (b [i]) > maxSig)
	      maxSig = abs (b [i]);
	}

	maxSig = 0.5 / maxSig;
	for (int i = 0; i < SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE; i++) {
	   samples_i [i] = maxSig * real (b [i]);
	   samples_q [i] = maxSig * imag (b [i]);
	}

	time_t unixtime; 
	time ( &unixtime );
	unixtime = unixtime - 120 + 1;
	rx_state. gtm = gmtime (&unixtime);

	int n_results = 0;
//	Search & decode the signal 
	wspr_decode (&fft,
	             samples_i,
	             samples_q,
	             SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE,
	             dec_options,
	             dec_results,
	             &n_results);
	time_t localtime;
	time(&localtime);
        localtime = localtime - 120 + 1;
	std::vector<struct decoder_results> resultStack;
	for (int i = 0; i < n_results; i ++)
	   if (!recentlySeen (dec_results [i], localtime)) 
	      resultStack. push_back (dec_results [i]);
	   printSpots (resultStack);
	if (rx_options. report && (resultStack. size () > 0)) 
	   postSpots (resultStack);
}

void	SDRunoPlugin_wspr::printSpots (std::vector<decoder_results> &results) {
time_t localtime;
struct tm	*gtm;

	time (&localtime);
	localtime = localtime - 120 + 1;
	gtm = gmtime (&localtime);
	std::string theTime	= std::to_string (gtm -> tm_hour) + "-" +
	                                     std::to_string (gtm -> tm_min);

	if (results. size () == 0) {
	   std::string message = "Nothing at " + theTime;
	   m_form. addMessage (message);
	   return;
	}

	printing. lock ();
	for (auto &res: results) {
	   std::string currentMessage  = "at " + theTime +"\t\t"+
	                    std::to_string (res. snr) + "\t" +
		            std::to_string (res. dt) + "\t" +
	                    std::to_string (res. freq) + "\t" +
	                    std::to_string (res. drift) + "\t" + 
	                    std::string (res. call) + "\t" +
	                    std::string (res. loc) + "\t" +
	                    std::string (res. pwr);
	   m_form. addMessage (currentMessage);
	   if (filePointer != nullptr) {
		   std::string currentMessage = "at " + theTime + ";" +
			   std::to_string (res. snr) + ";" +
			   std::to_string (res. dt) + ";" +
			   std::to_string (res. freq) + ";" +
			   std::to_string (res. drift) + ";" +
			   std::string (res. call) + ";" +
			   std::string (res. loc) + ";" +
			   std::string (res. pwr) + ";";
	      fprintf (filePointer, "%s\n", currentMessage. c_str ());
	   }
	}
	printing. unlock ();
}

//	Report on PSK reporter
void	SDRunoPlugin_wspr::postSpots (std::vector<decoder_results> &results) {

	if ((dec_options. rcall [0] == 0) ||
	    (dec_options. rloc  [0] == 0))
	   return;

	m_form. show_printStatus ("posting " + std::to_string (results. size ()));
	printing. lock ();
	for (auto &res : results) 
	   theWriter	-> addMessage (res);
	theWriter	-> sendMessages ();
	printing. unlock ();
}

struct {
	std::string bandName;
	int	dialFreq;
} freqTable [] = {
	{"LF",	136000},
	{"MF",	474200},
	{"160m", 1836600},
	{"80m", 3568600},
	{"60m", 5287200},
	{"40m", 7038600},
	{"30m", 10138700},
	{"20m", 14095600},
	{"17m", 18104600},
	{"15m", 21094600},
	{"12m", 24924600},
	{"10m", 28124600},
	{"6m",  50293000},
	{"4m",  70091000},
	{"2m",  144489000},
	{"1m15", 222280000},
	{"70cm", 432300000},
	{"23cm", 1296500000},
	{"", -1}
};

void	SDRunoPlugin_wspr::set_band (const std::string &s) {
	for (int i = 0; freqTable [i]. dialFreq != -1; i ++) {
	   if (freqTable [i]. bandName == s) {
	      rx_state. frequencyChange. store (true);
	      newFrequency	= freqTable [i]. dialFreq;
	      m_form. show_status ("change waiting");
	      return;
	   }
	}
}

void    SDRunoPlugin_wspr::set_callSign () {
nana::form fm;

	nana::inputbox::text callsign ("Callsign");
	nana::inputbox::text grid ("(maidenhead) grid");
	nana::inputbox::text antenna ("antenna");
	nana::inputbox inbox (fm, "please fill in");
	if (inbox. show (callsign, grid)) {
	   std::string n = callsign. value ();          //nana::string
	   std::string g = grid. value ();              //nana::string
	   std::string a = grid. value ();              //nana::string
	   if (n == "")
	      m_form. display_callsign ("no call");
	   else
	      m_form. display_callsign (n);
	   if (g == "")
	      m_form. display_grid ("no grid");
	   else
	      m_form. display_grid (g);
	   m_form. store_callSign (n);
	   m_form. store_grid     (g);
	   m_form. store_antenna  (a);

	   printing.lock();
	   for (int i = 0; i < 13; i ++)
	      dec_options. rcall [i] = 0;
	   for (int i = 0; i < n.size(); i++)
		   dec_options.rcall[i] = n.at(i);
	   for (int i = 0; i < 7; i ++)
	      dec_options. rloc [i] = 0;
	   for (int i = 0; i < g.size(); i++)
	      dec_options. rloc [i] = g.at(i);
	   printing.unlock();
	}
}

void	SDRunoPlugin_wspr::set_subtraction (bool b) {
	dec_options. subtraction	= b;
}

void	SDRunoPlugin_wspr::set_quickMode (bool b) {
	dec_options. quickmode	= b;
}

void	SDRunoPlugin_wspr::switch_reportMode () {
	printing. lock ();
	if (theWriter != nullptr) {
	   delete theWriter;     
	   theWriter = nullptr;
        }  
	else {
	   try {
	      theWriter = new reporterWriter (&m_form);
	   }
	   catch (int e) {
	   }
	}
	rx_options. report		= theWriter != nullptr;
	printing.unlock();
	m_form. show_reportMode (rx_options. report);
}

void	SDRunoPlugin_wspr::handle_reset	() {
	if (!rx_state. running. load ())
	   return;
	rx_state. savingSamples. store (false);
	rx_state. running. store (false);
	Sleep (200);
	m_worker -> join ();
	delete m_worker;
	m_worker	= nullptr;
	m_worker =
		new std::thread (&SDRunoPlugin_wspr::workerFunction, this);
}

bool	SDRunoPlugin_wspr::set_wsprDump	() {
	if (filePointer == nullptr) {
	   time_t now = time (0);
	   std::string currentTime = ctime (&now);
	   std::string defaultName =getenv ("HOMEPATH") + std::string ("\wspr_") + currentTime + ".csv";
	   nana::filebox fb (0, false);
	   fb. init_path (getenv ("HOMEPATH"));
//	   fb. init_file (defaultName);
	   fb. add_filter ("cv File", "*.csv");
//	   fb. add_filter("All Files", "*.*");
	   auto files = fb ();
	   if (!files. empty ()) {
	      printing. lock ();
	      filePointer =
	             fopen (files. front (). string (). c_str (), "w");
	      if (filePointer != nullptr){
	         std::string topLine =
	                  "; snr; dr; freq; drift; call; loc; power;" ;
	         fprintf (filePointer, "\n%s ; %s\n",
	                            topLine. c_str (), currentTime);
	      }
	      printing. unlock ();
	   }
	}
	else {
	   printing. lock ();
	   fclose (filePointer);
	   filePointer = nullptr;
	   printing. unlock ();
	}
	return filePointer != nullptr;
}


static
bool	equals (char *a, char *b) {
	return std::string(a) == std::string(b);
}

bool	SDRunoPlugin_wspr::recentlySeen (struct decoder_results &lastRes, 
	                                 time_t time) {
	while (resultQueue. front (). time + 2 * 300 < time)
	   resultQueue. pop_front ();

	for (auto &res : resultQueue) 
	  if (equals (lastRes. call, res. callRecord. call))
	      return true;

	listElement newEl;
	newEl. time = time;
	newEl. callRecord = lastRes;
	resultQueue. push_back (newEl);
	return false;
}
	   
