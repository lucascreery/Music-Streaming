#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/socket.h"

enum 
{
    wxID_BUTCONN = 0,
    wxID_DESC,
    wxID_SOCKET
};


class Api : public wxApp
{
    public:
        virtual bool OnInit();
};


class Window : public wxFrame
{
    protected: //Window Controls
        wxPanel *Panel;
        wxButton *ButConn;
        wxTextCtrl *Desc;
    public:
        Window(const wxString &title, const wxPoint &pos, const wxSize &size,
            long style = wxDEFAULT_FRAME_STYLE);
        void FunConnect(wxCommandEvent &evt);
        void OnSocketEvent(wxSocketEvent &evt);
    private:
	DECLARE_EVENT_TABLE()

};

//Event Table
BEGIN_EVENT_TABLE(Window, wxFrame)
EVT_BUTTON(wxID_BUTCONN, Window::FunConnect)
EVT_SOCKET(wxID_SOCKET, Window::OnSocketEvent)
END_EVENT_TABLE()

void Window::FunConnect(wxCommandEvent &evt)
{
    Desc->AppendText(_T("Connecting to the server...\n"));
    ButConn->Enable(FALSE);
    
    //Connecting to the server
    wxIPV4address adr;
    adr.Hostname(_T("localhost"));
    adr.Service(3000);
    
    //Create the socket
    wxSocketClient *Socket = new wxSocketClient();
    
    Socket->SetEventHandler(*this, wxID_SOCKET);
    Socket->SetNotify(wxSOCKET_CONNECTION_FLAG | wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
    Socket->Notify(TRUE);
    
    if(Socket->Connect(adr, false))
    {
        Desc->AppendText(_T("Connection successful\n\n"));
    }
    else
    {
        Desc->AppendText(_T("Connection failed\n\n"));
        ButConn->Enable(TRUE);
    }
    
    return;   
}

void Window::OnSocketEvent(wxSocketEvent &evt)
{
    wxSocketBase *Sock = evt.GetSocket();
    
    char buffer[10];
    
    switch(evt.GetSocketEvent())
    {
        case wxSOCKET_CONNECTION:
        {
            char mychar = '0';
            
            for(int i=0; i<10; ++i)
            {
                buffer[i] = mychar++;
            }
            
            Sock->Write(buffer, sizeof(buffer));
            
            break;
        }
        
        case wxSOCKET_INPUT:
        {
            Sock->Read(buffer, sizeof(buffer));
            break;
        }
        
        case wxSOCKET_LOST:
        {
            Sock->Destroy();
            break;
        }
    }
}

Window::Window(const wxString &title, const wxPoint &pos, const wxSize &size,
            long style) : wxFrame(NULL, -1, title, pos, size, style)
{
    Panel = new wxPanel(this, wxID_ANY);
    ButConn = new wxButton(Panel, wxID_BUTCONN, _T("Connect"), wxPoint(10, 10));
    Desc = new wxTextCtrl(Panel, wxID_DESC, wxEmptyString, wxPoint(10, 50), wxSize(300, 300), wxTE_MULTILINE | wxTE_BESTWRAP | wxTE_READONLY);
    
    Desc->SetValue("Welcome in my SocketDemo: Client\nClient Ready!\n\n");
}

bool Api::OnInit()
{
    Window *MainWindow = new Window(_T("SocketDemo: Client"), wxPoint(10, 10), wxSize(500, 500));
    MainWindow->Show(TRUE);
    return TRUE;   
}

IMPLEMENT_APP(Api)