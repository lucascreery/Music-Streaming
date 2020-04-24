#include <wx/wxprec.h>
#include <wx/listctrl.h>
#include <wx/socket.h>
#include <wx/slider.h>
#include <wx/filedlg.h>
#include <wx/wfstream.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <thread>

#include <stk/RtAudio.h>
#include <stk/SineWave.h>

enum {
    TITLE_LIST = 101,
    wxID_BUTTONPREV,
    wxID_BUTTONPLAY,
    wxID_BUTTONSTOP,
    wxID_BUTTONNEXT,
    wxID_BUTTONVD,
    wxID_BUTTONVU,
    wxID_SOCKET,
    wxID_SLIDER,
    wxID_DDOWNLOAD
};

class MainApp: public wxApp {
    public:
    virtual bool OnInit();
};

class audioBuffer{
    public:
    audioBuffer(int sr, int ch, size_t size) : buffering(true), samples(size), offset(0), 
                                            buffer_offset(0), sampleRate(sr), channels(ch) {
        buffer = new float[size];
    }
    ~audioBuffer(){
        free(buffer);
    }
    float tick(){
        if(!buffering && (buffer_offset - offset) == 0){
            buffering = true;
        }
        if(buffering){
            //printf("Buffering: %i\n", buffer.size());
            if((buffer_offset - offset) > sampleRate * channels || buffer_offset == samples){
                buffering = false;
            }else{
                return 0;
            }
        }
        float ret = buffer[offset];
        offset++;
        return ret;
    }
    void load(float *buf, size_t size){
        for(int i = 0; i < size; i++){
            //printf("Loading element %i out of %i\n", i, size);
            buffer[buffer_offset] = buf[i];
            buffer_offset++;
        }
    }
    float * buffer;
    private:
    bool buffering;
    int samples, offset, buffer_offset, sampleRate, channels;
};

int audioCallback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames, 
                    double streamTime, RtAudioStreamStatus status, void *dataPointer) {
    audioBuffer *mybuffer = (audioBuffer *) dataPointer;
    float *samples = (float *) outputBuffer;
    for(unsigned int i=0; i<nBufferFrames*2; i++){
        //printf("getting sample %i\n", i);
        *samples++ = mybuffer->tick();
    }
    return 0;
}

class wxMediaListCtrl : public wxListCtrl {
    public:
    void AddToMediaList(const wxString& titlest, const wxString& albumst, const wxString& artistst) {
        wxListItem kNewItem;
        kNewItem.SetAlign(wxLIST_FORMAT_LEFT);

        int nID = this->GetItemCount();
        kNewItem.SetId(nID);
        kNewItem.SetMask(wxLIST_MASK_DATA);
        kNewItem.SetData(new wxString(titlest));

        this->InsertItem(kNewItem);
        this->SetItem(nID, 0, titlest);
        this->SetItem(nID, 1, albumst);
        this->SetItem(nID, 2, artistst);
    }

    void GetSelectedItem(wxListItem& listitem) {
        listitem.SetMask(wxLIST_MASK_TEXT |  wxLIST_MASK_DATA);
        int nLast = -1, nLastSelected = -1;
        while ((nLast = this->GetNextItem(nLast, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1) {
            listitem.SetId(nLast);
            this->GetItem(listitem);
            if ((listitem.GetState() & wxLIST_STATE_FOCUSED) )
                break;
            nLastSelected = nLast;
        }
        if (nLast == -1 && nLastSelected == -1)
            return;
        listitem.SetId(nLastSelected == -1 ? nLast : nLastSelected);
        this->GetItem(listitem);
    }
};

class Window: public wxFrame {
    public:
    wxMediaListCtrl *titleListCtrl;
    wxBitmapButton *m_prevButton;
    wxBitmapButton *m_playButton;
    wxBitmapButton *m_stopButton;
    wxBitmapButton *m_nextButton;
    wxBitmapButton *m_vdButton;
    wxBitmapButton *m_vuButton;
    wxSlider *m_slider;
    wxSocketClient *Socket;
    wxSocketClient *StreamSocket;
    bool streaming, playing;
    std::thread streamthread;
    Window(const wxString& title, const wxPoint& pos, const wxSize& size);

    private:
    void updateMusicList();
    void OnStream(std::string songstream);
    void OnOpen(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnListBoxChoose(wxListEvent& event);
    void OnSocketEvent(wxSocketEvent& event);
    void OnPrevButton(wxCommandEvent& event);
    void OnPlayButton(wxCommandEvent& event);
    void OnStopButton(wxCommandEvent& event);
    void OnNextButton(wxCommandEvent& event);
    void OnVolDwnButton(wxCommandEvent& event);
    void OnVolUpButton(wxCommandEvent& event);
    void OnSeek(wxScrollEvent& event);
    void OnListRightClick(wxListEvent& event);
    void OnPopupClick(wxCommandEvent &evt);
    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(Window, wxFrame)
    EVT_MENU(wxID_OPEN,  Window::OnOpen)
    EVT_MENU(wxID_EXIT,  Window::OnExit)
    EVT_MENU(wxID_ABOUT, Window::OnAbout)
    EVT_LIST_ITEM_ACTIVATED(TITLE_LIST, Window::OnListBoxChoose)
    EVT_LIST_ITEM_RIGHT_CLICK(TITLE_LIST, Window::OnListRightClick)
    EVT_SOCKET(wxID_SOCKET, Window::OnSocketEvent)
    EVT_BUTTON(wxID_BUTTONPREV, Window::OnPrevButton)
    EVT_BUTTON(wxID_BUTTONPLAY, Window::OnPlayButton)
    EVT_BUTTON(wxID_BUTTONSTOP, Window::OnStopButton)
    EVT_BUTTON(wxID_BUTTONNEXT, Window::OnNextButton)
    EVT_BUTTON(wxID_BUTTONVD, Window::OnVolDwnButton)
    EVT_BUTTON(wxID_BUTTONVU, Window::OnVolUpButton)
    EVT_COMMAND_SCROLL_THUMBRELEASE(wxID_SLIDER, Window::OnSeek)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP(MainApp);

bool MainApp::OnInit() {
    Window *frame = new Window("Creery Media Streaming Client", wxPoint(50, 50), wxSize(450, 340) );
    frame->Show( true );
    return true;
}

Window::Window(const wxString& title, const wxPoint& pos, const wxSize& size)
        : wxFrame(NULL, wxID_ANY, title, pos, size,  wxMINIMIZE_BOX|wxSYSTEM_MENU|wxCAPTION|wxCLOSE_BOX|wxCLIP_CHILDREN) {
    playing = false;
    streaming = false;
    SetBackgroundColour(*wxWHITE);
    wxMenu *menuFile = new wxMenu;
    menuFile->Append(wxID_OPEN);
    menuFile->Append(wxID_EXIT);
    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append( menuFile, "&File" );
    menuBar->Append( menuHelp, "&Help" );
    SetMenuBar(menuBar);

    CreateStatusBar(2);
    SetStatusText("",0);

    wxBoxSizer *panelSizer = new wxBoxSizer(wxVERTICAL);
    titleListCtrl = new wxMediaListCtrl();
    titleListCtrl->Create(this, TITLE_LIST, wxDefaultPosition, wxSize(900,500), wxLC_REPORT|wxLC_HRULES|wxLC_NO_HEADER|wxLC_VRULES);
    titleListCtrl->AppendColumn(_("Title"), wxLIST_FORMAT_CENTER, 300);
    titleListCtrl->AppendColumn(_("Album"), wxLIST_FORMAT_CENTER, 300);
    titleListCtrl->AppendColumn(_("Artist"), wxLIST_FORMAT_CENTER, 300); 
    panelSizer->Add(titleListCtrl, wxSizerFlags(1).Expand());

    wxBoxSizer* horsizer1 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* vertsizer = new wxBoxSizer(wxHORIZONTAL);
    m_prevButton = new wxBitmapButton();
    m_playButton = new wxBitmapButton();
    m_stopButton = new wxBitmapButton();
    m_nextButton = new wxBitmapButton();
    m_vdButton = new wxBitmapButton();
    m_vuButton = new wxBitmapButton();
    wxImage i_prevButton("resources/icons8-skip-to-start-100.bmp", wxBITMAP_TYPE_ANY);
    i_prevButton.Rescale(50, 50);
    wxImage i_playButton("resources/icons8-play-100.bmp", wxBITMAP_TYPE_ANY);
    i_playButton.Rescale(50, 50);
    wxImage i_stopButton("resources/icons8-stop-100.bmp", wxBITMAP_TYPE_ANY);
    i_stopButton.Rescale(50, 50);
    wxImage i_nextButton("resources/icons8-end-100.bmp", wxBITMAP_TYPE_ANY);
    i_nextButton.Rescale(50, 50);
    wxImage i_vdButton("resources/icons8-low-volume-100.bmp", wxBITMAP_TYPE_ANY);
    i_vdButton.Rescale(50, 50);
    wxImage i_vuButton("resources/icons8-voice-100.bmp", wxBITMAP_TYPE_ANY);
    i_vuButton.Rescale(50, 50);
    m_prevButton->Create(this, wxID_BUTTONPREV, wxBitmap(i_prevButton), wxDefaultPosition, wxSize(50,50));
    m_prevButton->SetToolTip("Previous");
    m_playButton->Create(this, wxID_BUTTONPLAY, wxBitmap(i_playButton), wxDefaultPosition, wxSize(50,50));
    m_playButton->SetToolTip("Play");
    m_stopButton->Create(this, wxID_BUTTONSTOP, wxBitmap(i_stopButton), wxDefaultPosition, wxSize(50,50));
    m_stopButton->SetToolTip("Stop");
    m_nextButton->Create(this, wxID_BUTTONNEXT, wxBitmap(i_nextButton), wxDefaultPosition, wxSize(50,50));
    m_nextButton->SetToolTip("Next");
    m_vdButton->Create(this, wxID_BUTTONVD, wxBitmap(i_vdButton), wxDefaultPosition, wxSize(50,50));
    m_vdButton->SetToolTip("Volume down");
    m_vuButton->Create(this, wxID_BUTTONVU, wxBitmap(i_vuButton), wxDefaultPosition, wxSize(50,50));
    m_vuButton->SetToolTip("Volume up");
    vertsizer->Add(m_prevButton, 0, wxALIGN_CENTER_VERTICAL, 0);
    vertsizer->Add(m_playButton, 0, wxALIGN_CENTER_VERTICAL, 0);
    vertsizer->Add(m_stopButton, 0, wxALIGN_CENTER_VERTICAL, 0);
    vertsizer->Add(m_nextButton, 0, wxALIGN_CENTER_VERTICAL, 0);
    vertsizer->Add(m_vdButton, 0, wxALIGN_CENTER_VERTICAL, 0);
    vertsizer->Add(m_vuButton, 0, wxALIGN_CENTER_VERTICAL, 0);
    //horsizer1->Add(vertsizer, 0, wxALIGN_CENTER_VERTICAL, 0);
    panelSizer->Add(vertsizer, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    m_slider = new wxSlider(this, wxID_SLIDER, 0, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
    panelSizer->Add(m_slider, 0, wxEXPAND, 5);

    SetSizerAndFit(panelSizer);

    SetStatusText("Connection to Server ...", 0);
    wxIPV4address adr, localadr;
    if(!adr.Hostname(_T("192.168.1.218"))){
        printf("Invalid hostname/IP\n");
    }
    adr.Service(8890);
    localadr.AnyAddress();
    localadr.Service(8888);
    Socket = new wxSocketClient();
    Socket->SetEventHandler(*this, wxID_SOCKET);
    Socket->SetNotify(wxSOCKET_CONNECTION_FLAG | wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
    Socket->Notify(TRUE);
    if(Socket->Connect(adr, localadr, false)) {
        SetStatusText("Connection successful", 0);
    }else{
        SetStatusText("Connection failed", 0);
    }


}

void Window::OnOpen(wxCommandEvent& event){
    char tmpbuff[7], *contentbuf;

    SetStatusText("Choose File to Upload", 0);

    wxFileDialog openFileDialog(this, _("Upload Song"), "", "", 
                                "MP3 files (*.mp3)|*.mp3", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return;

    std::ifstream input_stream (openFileDialog.GetPath().wx_str(), std::ifstream::in|std::ifstream::binary);
    if (!input_stream.good()) {
        wxLogError("Cannot open file '%s'.", openFileDialog.GetPath());
        return;
    }

    SetStatusText("Uploading File...", 0);

    Socket->Write("1", 1);
    Socket->Read(tmpbuff, 5);
    //send file name
    Socket->Write(openFileDialog.GetFilename().c_str(), sizeof(openFileDialog.GetFilename().c_str()));
    //send file length
    std::filesystem::path fspath(openFileDialog.GetPath().ToStdString());
    auto fileSize = std::filesystem::file_size(fspath);
    contentbuf = new char[fileSize];
    std::stringstream lenstr;
    lenstr << std::filesystem::file_size(fspath);
    Socket->Write(lenstr.str().c_str(), sizeof(lenstr.str().c_str()));
    //recieve approval
    Socket->Read(tmpbuff, 7);
    tmpbuff[7] = '\0';
    if(strcmp(tmpbuff, "decline") == 0){
        SetStatusText("Upload Failed: File already exists", 0);
        return;
    }

    int offset = 0;
    input_stream.read(contentbuf, fileSize); 
    while(offset < fileSize){
        Socket->Write(contentbuf + offset, fileSize - offset);
        int amount = Socket->LastWriteCount();
        offset += amount;
    }
    Socket->Read(tmpbuff, 1);
    if(tmpbuff[1] = '1'){
        updateMusicList();
    }
    SetStatusText("Upload Successful", 0);
}

void Window::OnExit(wxCommandEvent& event) {
    Socket->Shutdown();
    streaming = false;
    streamthread.join();
    StreamSocket->Shutdown();
    Close(true);
}

void Window::OnAbout(wxCommandEvent& event) {
    wxMessageBox("My Media Streaming Client\rMade by Lucas Creery", "About", wxOK | wxICON_INFORMATION );
}

void Window::OnListBoxChoose(wxListEvent& event) {
    wxString message;
    message.Printf("Streaming %s", event.GetText());
    SetStatusText(message, 0);
    if(streaming){
        streaming = false;
        streamthread.join();
    }

    wxIPV4address adr, localadr;
    if(!adr.Hostname(_T("192.168.1.218"))){
        printf("Invalid hostname/IP\n");
    }
    adr.Service(8889);
    localadr.AnyAddress();
    localadr.Service(8889);
    StreamSocket = new wxSocketClient();
    StreamSocket->Notify(TRUE);
    StreamSocket->SetLocal(localadr);
    if(StreamSocket->Connect(adr, localadr)) {
        SetStatusText("Connection successful", 0);
    }else{
        SetStatusText("Connection failed", 0);
        return;
    }
    //const char *name = event.GetText().ToStdString().c_str();
    streamthread = std::thread(&Window::OnStream, this, event.GetText().ToStdString());
    //OnStream(event.GetText().ToStdString().c_str());
}

void Window::OnSocketEvent(wxSocketEvent& event){
//    wxSocketBase *sock = event.GetSocket();
    char buffer[2000];
    switch(event.GetSocketEvent()) {
        case wxSOCKET_CONNECTION:
            char buffer[2000];
            Socket->Read(buffer, sizeof(buffer));
            SetStatusText(buffer, 0);
            updateMusicList();
        break;
        
        case wxSOCKET_INPUT:
            //Socket->Read(buffer, sizeof(buffer));
        break;

        case wxSOCKET_LOST:
            Socket->Destroy();
            SetStatusText("Connection Lost", 0);
            exit(0);
        break;
    }
}

void Window::updateMusicList(){

    titleListCtrl->DeleteAllItems();

    SetStatusText("Requesting music list...", 0);
    char tmpmsg[6];
    Socket->Write("0",1);
    Socket->Read(tmpmsg, sizeof(tmpmsg));
    tmpmsg[Socket->LastReadCount()] = '\0';
    Socket->Write(tmpmsg, sizeof(tmpmsg));
    int fileCount;
    fileCount = atoi(tmpmsg);
    
    Socket->Read(tmpmsg, 1);
    if(atoi(tmpmsg) == 0){
        return;
    }

    char title[50], album[50], artist[50];
    //Socket->Read(tmp, dblen);
    //tmp[Socket->LastReadCount()] = '\0';
	for(int i = 0; i < fileCount; i++){
	    Socket->Read(title, 50);
        Socket->Write("1", 1);
        Socket->Read(album, 50);
        Socket->Write("1", 1);
	    Socket->Read(artist, 50);
        Socket->Write("1", 1);
        titleListCtrl->AddToMediaList(title, album, artist);
	}
    SetStatusText("Ready", 0);
}

void Window::OnPrevButton(wxCommandEvent& event){

}

void Window::OnPlayButton(wxCommandEvent& event){

}

void Window::OnStopButton(wxCommandEvent& event){

}

void Window::OnNextButton(wxCommandEvent& event){

}

void Window::OnVolDwnButton(wxCommandEvent& event){

}

void Window::OnVolUpButton(wxCommandEvent& event){

}

void Window::OnSeek(wxScrollEvent& event){

}

void Window::OnPopupClick(wxCommandEvent &evt){
	void *data=static_cast<wxMenu *>(evt.GetEventObject())->GetClientData();
    wxListItem selectedItem;
    titleListCtrl->GetSelectedItem(selectedItem);
 	wxString message;
    std::string filename = selectedItem.GetText().ToStdString();
    char servermsg[2000], *filebuff;
    int fileLen;
    std::ofstream outFile;

    switch(evt.GetId()){
 		case wxID_DDOWNLOAD:
        Socket->Write("2",1);
        Socket->Read(servermsg, 5);
        //send name of file
        Socket->Write(filename.c_str(), sizeof(filename.c_str()));
        //recieve file size
        Socket->Read(servermsg, 2000);
        servermsg[Socket->LastReadCount()] = '\0';
        fileLen = atoi(servermsg);
        if(fileLen == 0){
            SetStatusText("Download Failed", 0);
            break;
        }
        Socket->Write(servermsg, sizeof(servermsg));
        filebuff = (char*)malloc(sizeof(char) * fileLen);
        //Begin recieving file
        int offset;
        offset = 0;
        SetStatusText("Downloading", 0);
        while(offset < fileLen){
            Socket->Read(filebuff + offset, fileLen - offset);
            offset += Socket->LastReadCount();
        }
        //Save File
        filename.append(".mp3");
        wxFileDialog saveFileDialog(this, _("Save Song"), "", filename.c_str(), "(*.*)|*.*", wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
        if (saveFileDialog.ShowModal() == wxID_CANCEL){
            return;
            SetStatusText("Cancelled download", 0);
        }
        outFile = std::ofstream(saveFileDialog.GetPath().ToStdString(), std::ifstream::out|std::ifstream::binary);
        outFile.write(filebuff, fileLen);
        SetStatusText("File Saved Successfully", 0);

        outFile.close();
 		break;
 	}
}
 
void Window::OnListRightClick(wxListEvent &evt) {
 	void *data = reinterpret_cast<void *>(evt.GetItem().GetData());
 	wxMenu mnu;
 	mnu.SetClientData(data);
 	mnu.Append(wxID_DDOWNLOAD, 	"Download");
 	mnu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(Window::OnPopupClick), NULL, this);
 	PopupMenu(&mnu);
}

void Window::OnStream(std::string songstring){
    const char *song = songstring.c_str();
    printf("File: %s\n", song);
    char msg[2000];
    StreamSocket->Read(msg, sizeof(msg));
    SetStatusText(msg);

    StreamSocket->Write(song, sizeof(song));
    StreamSocket->Read(msg, 1);
    if(msg[0] == '0'){
        SetStatusText("Failed :(", 0);
        StreamSocket->Destroy();
        return;
    }
    //sample size
    int sampleSize;
    char sizechar[10];
    StreamSocket->Read(sizechar, 10);
    sizechar[StreamSocket->LastReadCount()] = '\0';
    StreamSocket->Write(sizechar, strlen(sizechar));
    sampleSize = atoi(sizechar);
    if(sampleSize == 0){
        SetStatusText("Error", 0);
        StreamSocket->Destroy();
        return;
    }
    printf("Sample Size:\t%i\n", sampleSize);
    //sample rate
    int sampleRate;
    char ratechar[10];
    StreamSocket->Read(ratechar, 10);
    ratechar[StreamSocket->LastReadCount()] = '\0';
    StreamSocket->Write(ratechar, strlen(ratechar));
    sampleRate = atoi(ratechar);
    printf("Sample Rate:\t%i\n", sampleRate);
    //channels
    int channels;
    char channelchar[2];
    StreamSocket->Read(channelchar, 2);
    channelchar[StreamSocket->LastReadCount()] = '\0';
    StreamSocket->Write(channelchar, strlen(channelchar));
    channels = atoi(channelchar);
    printf("Channels:\t%i\n", channels);
    //sample count
    float count;
    char countchar[50];
    StreamSocket->Read(countchar, 50);
    countchar[StreamSocket->LastReadCount()] = '\0';
    StreamSocket->Write(countchar, strlen(countchar));
    count = atof(countchar);
    printf("Sample Count:\t%f\n", count);
    audioBuffer mybuffer(sampleRate, channels, count);

    stk::Stk::setSampleRate(sampleRate);
    RtAudio dac;
    FILE * rawfile;
    // Figure out how many bytes in an StkFloat and setup the RtAudio stream.
    RtAudio::StreamParameters parameters;
    parameters.deviceId = dac.getDefaultOutputDevice();
    parameters.nChannels = channels;
    RtAudioFormat format = RTAUDIO_FLOAT32;
    unsigned int bufferFrames = stk::RT_BUFFER_SIZE;

    dac.openStream(&parameters, NULL, format, (unsigned int)stk::Stk::sampleRate(), &bufferFrames, &audioCallback, (void *)&mybuffer);
    dac.startStream();
    streaming = true;

    std::string nowPlaying = "Now Playing: ";
    nowPlaying.append(song);
    SetStatusText(nowPlaying, 0);

    //recieve audio data
    char *serverdata = new char[sampleRate * channels * sampleSize];
    float *bufferdata = new float[sampleRate * channels];
    int buffoffset = 0;
    while(buffoffset < count && streaming){
        StreamSocket->Read(serverdata, sampleRate * channels);
        int count = StreamSocket->LastReadCount();
        memcpy(bufferdata, serverdata, count);
        buffoffset += count;
        StreamSocket->Write("1", 1);
        mybuffer.load(bufferdata, count / sampleSize);
    }
    if(buffoffset < count){
        StreamSocket->WaitForWrite();
        StreamSocket->Write("0", 1);
    }
    free(serverdata);
    free(bufferdata);
    
    while(streaming){}
    printf("ending\n");
    dac.closeStream();

    StreamSocket->Close();
}