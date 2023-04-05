/**************************************************************************************\
*                                                                                      *
*              The Lisa Emulator Project  V1.2.7      DEV 2007.12.04                   *
*                             http://lisaem.sunder.net                                 *
*                                                                                      *
*                  Copyright (C) 1998, MMXX Ray A. Arachelian                          *
*                                All Rights Reserved                                   *
*                                                                                      *
*           This program is free software; you can redistribute it and/or              *
*           modify it under the terms of the GNU General Public License                *
*           as published by the Free Software Foundation; either version 2             *
*           of the License, or (at your option) any later version.                     *
*                                                                                      *
*           This program is distributed in the hope that it will be useful,            *
*           but WITHOUT ANY WARRANTY; without even the implied warranty of             *
*           MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
*           GNU General Public License for more details.                               *
*                                                                                      *
*           You should have received a copy of the GNU General Public License          *
*           along with this program;  if not, write to the Free Software               *
*           Foundation, Inc., 59 Temple Place #330, Boston, MA 02111-1307, USA.        *
*                                                                                      *
*                   or visit: http://www.gnu.org/licenses/gpl.html                     *
*                                                                                      *
\**************************************************************************************/

#include <wx/wx.h>
#include <wx/defs.h>
#include <wx/config.h>
#include <wx/event.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/notebook.h>

#include "machine.h"

#include "LisaConfig.h"
#include "LisaConfigFrame.h"

#include "machine.h"

#include <errno.h>

// from vars.c
extern "C" {
  extern int cheat_ram_test;
  extern int sound_effects_on;
  extern int skins_on_next_run;
  extern int hle;
  extern int macworks4mb;
  extern int double_sided_floppy;
  extern void save_configs(void);
  extern uint8 floppy_iorom;
  extern int consoletermwindow;
};

extern wxString get_config_filename(void);


extern void turn_skins_on(void);
extern void turn_skins_off(void);

extern "C" float hidpi_scale;

// fix for hidpi_scale <1.0 after 2020.01.26 changes
#define HIDPISCALE (hidpi_scale<1.0 ? 1.0 : hidpi_scale)


// Added as per Tom Frikker: see: https://lisalist2.com/index.php/topic,313.0.html for details
// the stored serial # is at offset 24 d0=sn2[1] * 100 + sn2[2] *10 + sn2[3];  // sn2[0]=0xff sync byte
/*
Another thing I noticed from los-deserialize.c (more of an improvement suggestion than a bug):
printf("Disk MDDF signed by Lisa SN: %08x (%d) zeroed out.",disk_sn,disk_sn);

Instead of printing disk_sn directly as decimal, it would probably be more useful to pull out the second two bytes, 
and print "001" followed by those parsed as decimal (preceded by as many 0s as it takes to get to 5 digits). 
This is the AppleNet number of the machine that did the serializing. Perhaps you could also add this number 
to the line that prints information about tools, as well?
*/

uint16 LisaConfigFrame::get_sn_stored_checksum_bytes(uint8 *sn2) { return (sn2[24] * 100 + sn2[25] *10 + sn2[26]); }

uint16 LisaConfigFrame::get_sn_checksum_bytes(uint8 *sn2) { // based on the "H" ROM Source code @ 0cb2
       uint16 d2=0;
       for  (int d1=0; d1<24; d1++) { d2 += (uint16)(sn2[d1] & 0x0f);}
       return d2+(uint16)(sn2[27] & 0x0f)-60; // wonder if the 60 is related to 60Hz?
}


void LisaConfigFrame::check_and_fix_serial_checksum(void)  // messagebox string of text, title
{
    uint8 sn2[32];                          //  0 1 2 3 4 5 6 7 8 9 a b c d e f
    wxString myserno=serialtxt->GetValue(); // ff000000000000ff0000000000000000
    wxString hexserno, decserno;            // FF999999999999FF9999999918995D2C
    unsigned long serno_i=0;

    fprintf(stderr,"Original Serial #",serno_i);
    for  (int i=0; i<32; i++) {
          hexserno=myserno.SubString(i,i); // serial number
          hexserno.ToULong(&serno_i,16);
          fprintf(stderr,"%01x",serno_i);
          sn2[i]=(uint8) serno_i & 0x0f;
    }
    fprintf(stderr,"\n");

    uint16 stored_checksum     = get_sn_stored_checksum_bytes(sn2);
    uint16 calculated_checksum = get_sn_checksum_bytes(sn2);

    if (stored_checksum != calculated_checksum ) {
        sn2[24]=calculated_checksum / 100; calculated_checksum = calculated_checksum % 100;
        sn2[25]=calculated_checksum / 10;  calculated_checksum = calculated_checksum % 10;
        sn2[26]=calculated_checksum;
        
        char *hex="0123456789ABCDEF";
        char fixed_serno[33];
        for (int i=0; i<32; i++) fixed_serno[i]=hex[sn2[i]];
        fixed_serno[32]=0;

        wxString fixedserno=fixed_serno;
        wxString text="The checksum in this serial #" + myserno + " has an incorrect checksum,\nA corrected Serial # would be:" + fixedserno +
                     " shall I correct it for you?";
        wxString title="Invalid Lisa Serial Number - Bad Checksum";
        wxMessageDialog w(this, text, title, wxICON_QUESTION  | wxYES_NO |wxNO_DEFAULT,wxDefaultPosition );

        if (w.ShowModal()==wxID_YES) { serialtxt->SetValue(fixedserno); }
    }
}


enum {
        ID_NOTEBOOK=2001,
        ID_APPLY,
        ID_PICK_ROM,
        ID_PICK_DPROM,
        ID_PICK_KB_B,
        ID_PICK_IOROM_B,
        ID_PICK_PPORT_B,
        ID_PICK_PROFILE,
        ID_PICK_PROFILES1H,
        ID_PICK_PROFILES1L,
        ID_PICK_PROFILES2H,
        ID_PICK_PROFILES2L,
        ID_PICK_PROFILES3H,
        ID_PICK_PROFILES3L,
        ID_PICK_PROFILESB1H,
        ID_PICK_PROFILESB1L,
        ID_PICK_PROFILESB2H,
        ID_PICK_PROFILESB2L,
        ID_PICK_PROFILESB3H,
        ID_PICK_PROFILESB3L,
        ID_PICK_IWDIR,
        ID_SERNO_INFO,
        ID_ZAP_PRAM,
        ID_SAVE_PRAM,
        ID_LOAD_PRAM
};

BEGIN_EVENT_TABLE(LisaConfigFrame, wxFrame)
  EVT_NOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK,  LisaConfigFrame::OnNoteBook)
  EVT_NOTEBOOK_PAGE_CHANGING(ID_NOTEBOOK, LisaConfigFrame::OnNoteBook)
  EVT_BUTTON(ID_SERNO_INFO,               LisaConfigFrame::OnSernoInfo)
  EVT_BUTTON(ID_APPLY,                    LisaConfigFrame::OnApply)
  EVT_BUTTON(ID_ZAP_PRAM,                 LisaConfigFrame::OnZapPram)
  EVT_BUTTON(ID_SAVE_PRAM,                LisaConfigFrame::OnSavePram)
  EVT_BUTTON(ID_LOAD_PRAM,                LisaConfigFrame::OnLoadPram)  

  EVT_BUTTON(ID_PICK_ROM,                 LisaConfigFrame::OnPickRom)
  EVT_BUTTON(ID_PICK_DPROM,               LisaConfigFrame::OnPickDRom)

  EVT_BUTTON(ID_PICK_PROFILE,             LisaConfigFrame::OnPickProFile)

  EVT_BUTTON(ID_PICK_PROFILESB1H,         LisaConfigFrame::OnPickProFile1H)
  EVT_BUTTON(ID_PICK_PROFILESB2H,         LisaConfigFrame::OnPickProFile2H)
  EVT_BUTTON(ID_PICK_PROFILESB3H,         LisaConfigFrame::OnPickProFile3H)
  EVT_BUTTON(ID_PICK_PROFILESB1L,         LisaConfigFrame::OnPickProFile1L)
  EVT_BUTTON(ID_PICK_PROFILESB2L,         LisaConfigFrame::OnPickProFile2L)
  EVT_BUTTON(ID_PICK_PROFILESB3L,         LisaConfigFrame::OnPickProFile3L)
  EVT_BUTTON(ID_PICK_IWDIR,               LisaConfigFrame::OnPickIWDir)
END_EVENT_TABLE()

const int idth[4]={0, ID_PICK_PROFILES1H,  ID_PICK_PROFILES2H,  ID_PICK_PROFILES3H};
const int idtl[4]={0, ID_PICK_PROFILES1L,  ID_PICK_PROFILES2L,  ID_PICK_PROFILES3L}; // why is this not used? bug?
const int idbh[4]={0, ID_PICK_PROFILESB1H, ID_PICK_PROFILESB2H, ID_PICK_PROFILESB3H};
const int idbl[4]={0, ID_PICK_PROFILESB1L, ID_PICK_PROFILESB2L, ID_PICK_PROFILESB3L};
// idth - upper text field idbh - button upper
// using wxID_ANY for lower for some reason(why?), idbl - button lower line ~493

LisaConfigFrame::LisaConfigFrame(const wxString& title, LisaConfig *lisaconfig)
       : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(630*HIDPISCALE,700*HIDPISCALE), 
                 wxDEFAULT_FRAME_STYLE|wxCLIP_CHILDREN |//|wxNO_FULL_REPAINT_ON_RESIZE)
                 wxMINIMIZE_BOX|wxMAXIMIZE_BOX|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCAPTION|
                 wxTAB_TRAVERSAL|wxCLOSE_BOX|wxNO_FULL_REPAINT_ON_RESIZE)
{

  my_lisaconfig = lisaconfig;
  serialabox = NULL;
  serialbbox = NULL;

  pportopts[0] = wxT("ProFile");
  pportopts[1] = wxT("ADMP");
  pportopts[2] = wxT("Nothing");

  wpportopts[0] = wxT("Widget-10");
  wpportopts[1] = wxT("ADMP");
  wpportopts[2] = wxT("Nothing");

  nothingonly[0]=_T("Nothing");
  nothingonly[1]=_T("Loopback");

  serportopts[0]=_T("Nothing");
  serportopts[1]=_T("Loopback");
  serportopts[2]=_T("Pipe");
  serportopts[3]=_T("File");
  serportopts[4]=_T("ImageWriter");
  serportopts[5]=_T("Terminal");

  #ifndef __MSVCRT__
   serportopts[6]=_T("TelnetD");
   serportopts[7]=_T("Shell");
   serportopts[8]=_T("Serial");
   serialopts=9;
  #else
   serialopts=6;
  #endif

  slotcard[0]=_T("Dual Parallel");
  slotcard[1]=_T("Nothing");

//  SetMinSize(wxSize(800,700)); //700,500));
//  SetSize(wxSize(800,700));

  thenoteBook =
            new wxNotebook(this, ID_NOTEBOOK,  wxDefaultPosition, wxSize(550, 650) );
  CreateNotebook(thenoteBook);
}

void LisaConfigFrame::OnNoteBook(wxNotebookEvent& WXUNUSED(event))
     {
      // The LisaTest loopback adapter attaches to both serial ports at once.
      // if one is set to loopback so, must the other follow.
      if (!serialabox || !serialbbox) return;

      if (serialabox->GetSelection()==1 || serialbbox->GetSelection()==1)
      {
       serialabox->SetSelection(1);
       serialbbox->SetSelection(1);
      }

     }

extern "C" uint8 floppy_ram[2048];
extern "C" int islisarunning(void);
//extern "C" void get_lisa_serialnumber(uint32 *plant, uint32 *year, uint32 *day, uint32 *sn, uint32 *prefix, uint32 *net)

void  LisaConfigFrame::OnSernoInfo(wxCommandEvent& WXUNUSED(event))
{
    wxString myserno=serialtxt->GetValue();
    wxString hexserno, decserno;
    wxString text, trailer;
    unsigned long serno_i=0;

//    uint32 plant, year, day, sn, prefix, net;
//    get_lisa_serialnumber(&plant, &year, &day, &sn, &prefix, &net);

    hexserno=_T("0x") + myserno.SubString(9,12); // serial number
    hexserno.ToULong(&serno_i,16);
    decserno << serno_i;
    
    if      (myserno.SubString(8,8)=='1') trailer=_T("st");
    else if (myserno.SubString(8,8)=='2') trailer=_T("nd");
    else if (myserno.SubString(8,8)=='3') trailer=_T("rd");
    else                                  trailer=_T("th");

//                                               01 23 45 67 89 ab cd ef 01 23 45 67 89 ab
//floppy.c:get_lisa_serialnumber:1532:serial240: ff 02 83 08 10 40 50 ff 00 10 16 35 04 70 00 00
//                                               XX XX XP XP XY XY XD XD XD XS XS XS XS XX XX XX
//                                                      3  8 |0  0 |0  f  0| 0  6  5  4 |

    text =
    _T("Your Lisa's serial number was built\nin Apple Plant #")+
    myserno.SubString(2,3) +  // plant code
    _T(" on the ")+
    myserno.SubString(6,8) + // day of year
    trailer +_T(" day of 19")+
    myserno.SubString(4,5) + // year
    _T("\nwith serial #")+
    myserno.SubString(9,12)+ // serial number
    _T(" (") + decserno + _T(")\n")+
    _T("It has the applenet id: ")+
    myserno.SubString(16,18)+ // applenet prefix
    _T(":")+
    myserno.SubString(19,23); // applenet node number

    wxMessageBox(text,_T("About your Lisa's Serial Numer"), wxICON_INFORMATION | wxOK);
    
}

void  LisaConfigFrame::OnSavePram(wxCommandEvent& WXUNUSED(event))
{
    char *filename;
    FILE *F;

    wxFileDialog x(NULL,                        wxT("Save the Lisa PRAM to a file"),
                                                wxEmptyString,
                                                wxT("lisaem.pram"),
                                                wxT("PRAM (*.pram)|*.pram|All (*.*)|*.*"),
                                                (long int)wxFD_SAVE|wxFD_OVERWRITE_PROMPT,wxDefaultPosition);

    if (x.ShowModal()==wxID_OK) filename=(char *)(const char *)(x.GetPath().c_str());
    else return;
    errno=0;
    F=fopen(filename,"wb");
    if (!F) {
              wxMessageBox(_T("Could not open the PRAM file for writing."),_T("File Error!"), wxICON_INFORMATION | wxOK);
              return;
            }
        

    fwrite(&floppy_ram[0x180/2],(0x200-0x180)/2,1,F);
    if (errno)
       {
          wxMessageBox(_T("An error occured while attempting to write to the PRAM file.  The PRAM was not saved!"),
                       _T("Save Error!"), wxICON_INFORMATION | wxOK);
       }

    fclose(F);
}


void  LisaConfigFrame::OnLoadPram(wxCommandEvent& WXUNUSED(event))
{
    char *filename;
    FILE *F;
    int count=0;
    uint8 backup[(0x200-0x180)/2];

    if (islisarunning())
       {
          wxMessageBox(_T("Cannot Load PRAM while the Lisa is running."),_T("Lisa is running!"), wxICON_INFORMATION | wxOK);
          return;
       }

    wxString text=wxT("Loading the PRAM will overwrite the current PRAM with the data saved in a file!\n\n"
                      "You should only use this in emergencies as this can cause more problems than fix.  "
                      "If you say Yes, this will happen even if you do not press the Apply button on the Preferences window and it cannot be undone!\n"
                      "\n\nReally Load PRAM?");
    wxString title=wxT("Load PRAM?");
    wxMessageDialog w(this,text, title, wxICON_QUESTION  | wxYES_NO |wxNO_DEFAULT,wxDefaultPosition );
    if (w.ShowModal()!=wxID_YES) return;

    wxFileDialog x(NULL,                        wxT("Load a Lisa PRAM file"),
                                                wxEmptyString,
                                                wxT("lisaem.pram"),
                                                wxT("PRAM (*.pram)|*.pram|All (*.*)|*.*"),
                                                (long int)wxFD_OPEN|wxFD_FILE_MUST_EXIST,wxDefaultPosition);

    if (x.ShowModal()==wxID_OK) filename=(char *)(const char *)(x.GetPath().c_str());
    else return;
    
    memcpy(backup,&floppy_ram[0x180/2],(0x200-0x180)/2);  // make a backup
    errno=0;
    F=fopen(filename,"rb");
    if (!F) {
              wxMessageBox(_T("Could not open the PRAM file for reading."),_T("File Error!"), wxICON_INFORMATION | wxOK);
              return;
            }
        
    count=fread(&floppy_ram[0x180/2],(0x200-0x180)/2,1,F);
    if (errno || count!=1)
       {
          memcpy(&floppy_ram[0x180/2],backup,(0x200-0x180)/2);  // restore the backup if there was a failure.
          wxMessageBox(_T("An error occured while attempting to read from the PRAM file.  The PRAM was not overwrriten."),
                       _T("Load Error!"), wxICON_INFORMATION | wxOK);
       }
    fclose(F);
}


void  LisaConfigFrame::OnZapPram(wxCommandEvent& WXUNUSED(event))
{
    if (islisarunning())
       {
          wxMessageBox(_T("Cannot Zap PRAM while the Lisa is running."),_T("Lisa is running!"), wxICON_INFORMATION | wxOK);
          return;
       }

    wxString text=wxT("Zapping the PRAM will zero out the Parameter RAM.\n\n"
                      "You should only use this in emergencies as this can cause more problems than fix.  "
                      "If you say Yes, this will happen even if you do not press the Apply button on the Preferences window.\n"
                      "\n\nReally ZAP PRAM?");
    wxString title=wxT("Zap PRAM?");
    wxMessageDialog w(this,text, title, wxICON_QUESTION  | wxYES_NO |wxNO_DEFAULT,wxDefaultPosition );
    if (w.ShowModal()!=wxID_YES) return;
        
    memset(&floppy_ram[0x180/2],0,(0x200-0x180)/2);
}


void  LisaConfigFrame::OnApply(wxCommandEvent& WXUNUSED(event))
{
 // --- configuration page ----------------------------------------------

 // if it hasn't been built yet, don't touch!
 if (!serialtxt)           return;
 if (!m_rompath)           return;
 if (!kbbox)               return;
 if (!serialabox)          return;
 if (!serialbbox)          return;
 if (!serialaxon)          return;
 if (!serialbxon)          return;
 if (!m_propath)           return;
 if (!pportbox)            return;

 if (!m_text_propathh[1])  return;
 if (!m_text_propathl[1])  return;
 if (!m_text_propathh[2])  return;
 if (!m_text_propathl[2])  return;
 if (!m_text_propathh[3])  return;
 if (!m_text_propathl[3])  return;

 check_and_fix_serial_checksum();

 my_lisaconfig->myserial= serialtxt->GetValue();
 my_lisaconfig->rompath = m_rompath->GetValue();
 my_lisaconfig->dualrom =m_dprompath->GetValue();

 my_lisaconfig->serial1xon=serialaxon->GetValue() ? "1":"0";
 my_lisaconfig->serial2xon=serialbxon->GetValue() ? "1":"0";
 
 consoletermwindow = console_term->GetValue() ? 1:0;
 /*
 fprintf(stderr,"myserial number:%s\n",my_lisaconfig->myserial.c_str());
 fprintf(stderr,"rompath        :%s\n",my_lisaconfig->rompath.c_str());
 fprintf(stderr,"dual rom path  :%s\n",my_lisaconfig->dualrom.c_str());
 */

 uint16 kbids[]={ 0xBF2f, 0xAF2f, 0xAD2d, 0xAE2e };
 my_lisaconfig->kbid=kbids[kbbox->GetSelection()];


 uint8 ioromids[]={ 0xa8, 0x88, 0x89, 0xa9, 0x40 };
 my_lisaconfig->iorom=ioromids[iorombox->GetSelection()];


 int memsizes[]={512,1024,1536,2048};
 my_lisaconfig->mymaxlisaram=memsizes[cpurambox->GetSelection()];
 cheat_ram_test=cheats->GetValue() ?1:0;

 hle=hle_cheats->GetValue() ?1:0;
 macworks4mb=0; // doesn't work yet // macwx4mb->GetValue() ? 1:0;

 sound_effects_on = soundeffects->GetValue()?1:0;
 double_sided_floppy=doublesided->GetValue() ?1:0;
 
 int last_skins_on=skins_on_next_run;   // disable this to require restart.
 skins_on_next_run= skinson->GetValue()?1:0;

 if (skins_on_next_run!=last_skins_on)
 {
    if (skins_on_next_run)   turn_skins_on();
    else                     turn_skins_off();
 }


 // --- ports ------------------------------------------------------------
 my_lisaconfig->serial1_setting=serportopts[serialabox->GetSelection()];
 my_lisaconfig->serial2_setting=serportopts[serialbbox->GetSelection()];
 my_lisaconfig->serial1_param  =serialaparam->GetValue();
 my_lisaconfig->serial2_param  =serialbparam->GetValue();

 /*
 fprintf(stderr,"serial a setting:%s\n",my_lisaconfig->serial1_setting.c_str());
 fprintf(stderr,"serial b setting:%s\n",my_lisaconfig->serial2_setting.c_str());
 fprintf(stderr,"serial a param  :%s\n",my_lisaconfig->serial1_param.c_str());
 fprintf(stderr,"serial a param  :%s\n",my_lisaconfig->serial2_param.c_str());
 */

 my_lisaconfig->parallel=pportopts[pportbox->GetSelection()];
 my_lisaconfig->parallelp=m_propath->GetValue();

 //fprintf(stderr,"parallel port   :%s\n",my_lisaconfig->parallel.c_str());
 //fprintf(stderr,"parameter       :%s\n\n",my_lisaconfig->parallelp.c_str());
 //fflush(stderr);



 // --- slots ------------------------------------------------------------

    my_lisaconfig->slot1=slotcard[sloton[1]->GetSelection()];
    my_lisaconfig->slot2=slotcard[sloton[2]->GetSelection()];
    my_lisaconfig->slot3=slotcard[sloton[3]->GetSelection()];

    my_lisaconfig->s1h=pportopts[pportboxh[1]->GetSelection()];
    my_lisaconfig->s1l=pportopts[pportboxl[1]->GetSelection()];
    my_lisaconfig->s2h=pportopts[pportboxh[2]->GetSelection()];
    my_lisaconfig->s2l=pportopts[pportboxl[2]->GetSelection()];
    my_lisaconfig->s3h=pportopts[pportboxh[3]->GetSelection()];
    my_lisaconfig->s3l=pportopts[pportboxl[3]->GetSelection()];

    my_lisaconfig->s1hp=m_text_propathh[1]->GetValue();
    my_lisaconfig->s1lp=m_text_propathl[1]->GetValue();
    my_lisaconfig->s2hp=m_text_propathh[2]->GetValue();
    my_lisaconfig->s2lp=m_text_propathl[2]->GetValue();
    my_lisaconfig->s3hp=m_text_propathh[3]->GetValue();
    my_lisaconfig->s3lp=m_text_propathl[3]->GetValue();

 // --- imagewriter settings ---------------------------------------------
   my_lisaconfig->iw_dipsw_1=(dipsw1_123->GetSelection() ) |
                             (dipsw1_4->GetSelection()<<3) |
                             (dipsw1_5->GetSelection()<<4) |
                             (dipsw1_67->GetSelection()<<5)|
                             (dipsw1_8->GetValue() ? 128:0 );
   my_lisaconfig->iw_png_on =iw_img_box->GetValue();
   my_lisaconfig->iw_png_path =iw_img_path->GetValue();


   save_configs();
   Close();
}





wxPanel *LisaConfigFrame::CreateSlotConfigPage(wxNotebook *parent, int slot)
{
    int y=10* HIDPISCALE, ya=50* HIDPISCALE;

    if ( slot<1 || slot>3) return NULL;

    wxPanel *panel = new wxPanel(parent);

    wxString u,l, cu, cl,s ;

    switch (slot)
     {
        case 1: s=my_lisaconfig->slot1;u=my_lisaconfig->s1hp; l=my_lisaconfig->s1lp; cu=my_lisaconfig->s1h; cl=my_lisaconfig->s1l; break;
        case 2: s=my_lisaconfig->slot2;u=my_lisaconfig->s2hp; l=my_lisaconfig->s2lp; cu=my_lisaconfig->s2h; cl=my_lisaconfig->s2l; break;
        case 3: s=my_lisaconfig->slot3;u=my_lisaconfig->s3hp; l=my_lisaconfig->s3lp; cu=my_lisaconfig->s3h; cl=my_lisaconfig->s3l; break;
     }

    sloton[slot] = new wxRadioBox(panel, wxID_ANY,
        wxT("slot:"), wxPoint( 10* HIDPISCALE,  y),  wxDefaultSize, 2, slotcard, 1, wxRA_SPECIFY_COLS,
        wxDefaultValidator, wxT("radioBox"));                             y+=ya;
                                                                          y+=ya;
    if (s.IsSameAs(slotcard[0],false)) sloton[slot]->SetSelection(0); else sloton[slot]->SetSelection(1);


    //----- upper slot -------------------------------------------------------------------------------------------------

    pportboxh[slot] = new wxRadioBox(panel, wxID_ANY,
        wxT("Upper Parallel Port: (Connector 2 in LOS)"), wxPoint( 10* HIDPISCALE,  y), wxDefaultSize, 3, pportopts, 0, wxRA_SPECIFY_COLS,
        wxDefaultValidator, wxT("radioBox"));                             y+=ya;

    // default to nothing for these.
    if      (cu.IsSameAs(_T("PROFILE"),false))          pportboxh[slot]->SetSelection(0);
    else if (cu.IsSameAs(_T("ADMP"),false))             pportboxh[slot]->SetSelection(1);
    else                                                pportboxh[slot]->SetSelection(2);

    m_text_propathh[slot] = new wxTextCtrl(panel, idth[slot], u, wxPoint( 10,  y), wxSize( 400 * HIDPISCALE, 30 * HIDPISCALE), 0 );
    (void) new wxButton( panel, idbh[slot], wxT("browse"),  wxPoint( 420* HIDPISCALE,  y), wxDefaultSize);

                                                                          y+=ya;
                                                                          y+=ya;

    //----- lower slot --------------------------------------------------------------------------------------------------

    pportboxl[slot] = new wxRadioBox(panel, wxID_ANY,
        wxT("Lower Parallel Port: (Connector 1 in LOS)"), wxPoint(10,y), wxDefaultSize, 3, pportopts, 0, wxRA_SPECIFY_COLS,
        wxDefaultValidator, wxT("radioBox"));                             y+=ya;

    // default to nothing for these.
    if      (cl.IsSameAs(_T("PROFILE"),false))          pportboxl[slot]->SetSelection(0);
    else if (cl.IsSameAs(_T("ADMP"),false))             pportboxl[slot]->SetSelection(1);
    else                                                pportboxl[slot]->SetSelection(2);
    // 20200320 bug?                         was  wxID_ANY v
    m_text_propathl[slot] = new wxTextCtrl(panel, idtl[slot], l, wxPoint( 10 * HIDPISCALE,  y), wxSize( 400 * HIDPISCALE, 30 * HIDPISCALE ), 0 );
    (void) new wxButton( panel, idbl[slot], wxT("browse"),     wxPoint(420 * HIDPISCALE , y), wxDefaultSize);

    // idth - upper text field idbh - button upper
    // using wxID_ANY for lower for some reason(why?), idbl - button lower line ~493


    (void) new wxButton( panel, ID_APPLY, wxT("Apply"), applypoint, wxDefaultSize );

    return panel;
}


wxPanel *LisaConfigFrame::CreateMainConfigPage(wxNotebook *parent)
{
   wxPanel *panel = new wxPanel(parent);

   int y=10* HIDPISCALE,  ya=50* HIDPISCALE;

   // Tell the user what config file we're using.
   wxString t;
   t=_T("Prefs file: ") + get_config_filename();

   (void)new wxStaticText(panel,wxID_ANY,   t,      wxPoint( 10* HIDPISCALE,  y), wxSize(500* HIDPISCALE, 30* HIDPISCALE));    y+=ya/2; //y+=ya/2;

    wxString ramsize[] = { wxT("0.5 MB"), wxT("1 MB"), wxT("1.5 MB"),  wxT("2 MB*") };

    cpurambox = new wxRadioBox(panel, wxID_ANY,wxT("RAM:"), wxPoint(10 * HIDPISCALE,y), wxDefaultSize, 
    #ifdef ALLOW2MBRAM
          4, // 3 to turn off 2mb// 4 - to -reneable 2MB, uncomment case 2048 below as well
    #else
          3,
    #endif
       ramsize, 0, wxRA_SPECIFY_COLS,
       wxDefaultValidator, wxT("radioBox"));


    switch(my_lisaconfig->mymaxlisaram)
    {
     case  512:  cpurambox->SetSelection(0); break;
     case 1024:  cpurambox->SetSelection(1); break;
     case 1536:  cpurambox->SetSelection(2); break;
     #ifdef ALLOW2MBRAM
     case 2048:  cpurambox->SetSelection(3); break;
     #endif
     default:    cpurambox->SetSelection(2); 
    }

   // doesn't work yet
   //macwx4mb = new wxCheckBox(panel, wxID_ANY, wxT("4MB RAM MacWorks"), wxPoint(320 * HIDPISCALE,y+(ya/4)), wxDefaultSize,wxCHK_2STATE);
   //macwx4mb->SetValue( (bool)(macworks4mb) );

   y+=ya; y+=ya/2;

   (void)new wxStaticText(panel, wxID_ANY, _T("Lisa Serial Number:"),      wxPoint( 10* HIDPISCALE,  y), wxSize(400 * HIDPISCALE, 30 * HIDPISCALE));    y+=(ya/2);

   (void) new wxButton( panel, ID_SERNO_INFO,  wxT("info"),  wxPoint(420 * HIDPISCALE,  y), wxDefaultSize );

    serialtxt = new wxTextCtrl(panel, wxID_ANY,  my_lisaconfig->myserial,  wxPoint( 10* HIDPISCALE,  y), wxSize(400 * HIDPISCALE, 30 * HIDPISCALE), 0); y+=ya;


   (void)new wxStaticText(panel, wxID_ANY, _T("Lisa ROM:"),                wxPoint( 10* HIDPISCALE,  y), wxSize(400 * HIDPISCALE, 30 * HIDPISCALE));    y+=(ya/2);
    m_rompath = new wxTextCtrl(panel, wxID_ANY,  my_lisaconfig->rompath,   wxPoint( 10* HIDPISCALE,  y), wxSize(400 * HIDPISCALE, 30 * HIDPISCALE),0);
    b_rompath = new wxButton( panel, ID_PICK_ROM, wxT("browse"),           wxPoint(420* HIDPISCALE,  y), wxDefaultSize );     y+=ya;


   (void)new wxStaticText(panel, wxID_ANY, _T("Dual Parallel Card ROM:"),  wxPoint( 10,  y), wxSize(400 * HIDPISCALE, 30 * HIDPISCALE));    y+=(ya/2);
    m_dprompath = new wxTextCtrl(panel, wxID_ANY, my_lisaconfig->dualrom,  wxPoint( 10,  y), wxSize(400 * HIDPISCALE, 30 * HIDPISCALE),0);
    b_dprompath = new wxButton( panel, ID_PICK_DPROM, wxT("browse"),       wxPoint(420,  y), wxDefaultSize );     y+=ya;


    wxString kbid[] = { wxT("US"), wxT("UK"), wxT("FR"), wxT("DE")};
    kbbox = new wxRadioBox(panel, wxID_ANY, wxT("Keyboard:"), wxPoint(10, y), wxDefaultSize, 4, kbid, 0, wxRA_SPECIFY_COLS,
                           wxDefaultValidator, wxT("radioBox"));  // y+=ya+ya/2;
    switch(my_lisaconfig->kbid)
    {
     case 0xAD2d:   kbbox->SetSelection(2); break;
     case 0xAE2e:   kbbox->SetSelection(3); break;
     case 0xAF2f:   kbbox->SetSelection(1); break;
     case 0xBF2f:
     default:       kbbox->SetSelection(0);
    }

    wxString iorom[] = { wxT("A8"), wxT("88"), wxT("89"), wxT("A9"), wxT("40") };
    iorombox = new wxRadioBox(panel, wxID_ANY,wxT("I/O ROM:"), wxPoint(320 * HIDPISCALE,y), wxDefaultSize, 5, iorom, 0, wxRA_SPECIFY_COLS,
       wxDefaultValidator, wxT("radioBox"));  y+=ya+(ya>>1);
    switch(my_lisaconfig->iorom)
    {
     case 0x88 : iorombox->SetSelection(1); break;
     case 0x89 : iorombox->SetSelection(2); break;
     case 0xa9 : iorombox->SetSelection(3); break;
     case 0x40 : iorombox->SetSelection(4); break;
     case 0xa8 :
     default:    iorombox->SetSelection(0);
    }

    doublesided = new wxCheckBox(panel, wxID_ANY, wxT("SunRem 2x Sided Sony"), wxPoint(320 * HIDPISCALE,y-(ya/2)    ), wxDefaultSize,wxCHK_2STATE);
    doublesided->SetValue( (bool)(double_sided_floppy) );  // y+=ya/2;

    soundeffects = new wxCheckBox(panel, wxID_ANY, wxT("Sound Effects"), wxPoint(10 * HIDPISCALE,y), wxDefaultSize,wxCHK_2STATE);
    soundeffects->SetValue( (bool)(sound_effects_on) );  y+=ya/2;
    int yz=y;

    skinson = new wxCheckBox(panel, wxID_ANY, wxT("Lisa Skins"), wxPoint(10 * HIDPISCALE,y), wxDefaultSize,wxCHK_2STATE);
    skinson->SetValue( (bool)(skins_on_next_run) );  y+=ya/2;

    cheats = new wxCheckBox(panel, wxID_ANY, wxT("Boot ROM speedup hacks"), wxPoint(10 * HIDPISCALE,y), wxDefaultSize,wxCHK_2STATE);
    cheats->SetValue( (bool)(cheat_ram_test) );

    hle_cheats  = new wxCheckBox(panel, wxID_ANY, wxT("Hard Drive Acceleration"), wxPoint(10 * HIDPISCALE,y+ya/2), wxDefaultSize,wxCHK_2STATE);
    hle_cheats->SetValue( (bool)(hle) );   y+=ya/2;

    console_term = new wxCheckBox(panel, wxID_ANY, wxT("Console Terminal"), wxPoint(10 * HIDPISCALE,y+ya/2), wxDefaultSize,wxCHK_2STATE);
    console_term->SetValue( (bool) consoletermwindow );

    applypoint=wxPoint(420 * HIDPISCALE,  yz+ya);
    (void) new wxButton( panel, ID_APPLY, wxT("Apply"), applypoint, wxDefaultSize );

    (void)new wxStaticText(panel, wxID_ANY,  _T("PRAM:"),  wxPoint(320 * HIDPISCALE, (yz)-ya/2), wxSize(400 * HIDPISCALE, 30 * HIDPISCALE));
    (void) new wxButton( panel, ID_SAVE_PRAM, wxT("Save"), wxPoint(320 * HIDPISCALE,  yz), wxDefaultSize );
    (void) new wxButton( panel, ID_LOAD_PRAM, wxT("Load"), wxPoint(400 * HIDPISCALE,  yz), wxDefaultSize );
    (void) new wxButton( panel, ID_ZAP_PRAM,  wxT("Zap"),  wxPoint(480 * HIDPISCALE,  yz), wxDefaultSize );

    return panel;
}


wxPanel *LisaConfigFrame::CreatePortsConfigPage(wxNotebook *parent)
{
    wxPanel *panel = new wxPanel(parent);
    //wxPanel *panel = new wxPanel(parent,wxID_ANY,wxDefaultPosition,wxSize(320,200),wxT("ports"));
    int y=10 * HIDPISCALE, ya=35 * HIDPISCALE;
    int i;


   (void)new wxStaticText(panel, wxID_ANY, _T("Serial A:"),  wxPoint( 10,  y+10), wxSize(100 * HIDPISCALE, 30 * HIDPISCALE));

#ifndef ALLOWSERIALA
    serialabox = new wxChoice(panel, wxID_ANY, wxPoint(100 * HIDPISCALE, y), wxDefaultSize, 1 /* serialopts */, nothingonly); y+=ya;
    serialabox->SetSelection(0);
#else
    serialabox = new wxChoice(panel, wxID_ANY, wxPoint(100 * HIDPISCALE, y), wxDefaultSize, serialopts, serportopts); y+=ya+ya;
     for (i=0; i<serialopts; i++)
         if (my_lisaconfig->serial1_setting.IsSameAs(serportopts[i],false) ) serialabox->SetSelection(i);
#endif

    //y+=ya/8;
    serialaparam = new wxTextCtrl(panel, wxID_ANY,  my_lisaconfig->serial1_param  ,
                         wxPoint(10 * HIDPISCALE, y), wxSize(380 * HIDPISCALE,30 * HIDPISCALE) , 0);

    serialaxon = new wxCheckBox(panel,wxID_ANY,  wxT("Xon/off"),
                        wxPoint(430 * HIDPISCALE, y-(HIDPISCALE*8)), wxSize(300 * HIDPISCALE,50 * HIDPISCALE) , 0);
    serialaxon->SetValue((bool) (my_lisaconfig->serial1xon.IsSameAs(_T("1"), false)) );            y+=ya*2;


   (void)new wxStaticText(panel, wxID_ANY, _T("Serial B:"),  wxPoint( 10,  y+10), wxSize(100 * HIDPISCALE, 30 * HIDPISCALE));
    serialbbox = new wxChoice(panel, wxID_ANY, wxPoint(100 * HIDPISCALE, y), wxDefaultSize, serialopts, serportopts); y+=ya;  //wxSize(380 * HIDPISCALE,128 * HIDPISCALE)
//    serialbbox = new wxRadioBox(panel, wxID_ANY,
//        wxT("Serial B:"), wxPoint(10 * HIDPISCALE, y), wxSize(380 * HIDPISCALE,128 * HIDPISCALE), serialopts, serportopts, 2, wxRA_SPECIFY_COLS,
//        wxDefaultValidator, wxT("radioBox"));                                                       y+=ya+ya;

   // y+=ya/8;
    serialbparam = new wxTextCtrl(panel, wxID_ANY,  my_lisaconfig->serial2_param  , wxPoint(10, y), wxSize(380 * HIDPISCALE,30 * HIDPISCALE) , 0);

    serialbxon = new wxCheckBox(panel,wxID_ANY, wxT("Xon/Off"),
                        wxPoint(430 * HIDPISCALE, y-(HIDPISCALE*12)), wxDefaultSize,  0); // wxSize(300 * HIDPISCALE, 60 * HIDPISCALE) 
    serialbxon->SetValue((bool) (my_lisaconfig->serial2xon.IsSameAs(_T("1"), false)));             

    for (i=0; i<serialopts; i++)
        if (my_lisaconfig->serial2_setting.IsSameAs(serportopts[i],false) ) serialbbox->SetSelection(i);
    
    y+=ya*2;



    if (floppy_iorom!=0x88)
    {
        pportbox = new wxRadioBox(panel, wxID_ANY,
            wxT("Parallel Port:"), wxPoint(10, y), wxDefaultSize, 3, pportopts, 0, wxRA_SPECIFY_COLS,
            wxDefaultValidator, wxT("radioBox"));                             
            my_lisaconfig->parallelp.Replace(_T("widget"),_T("profile"),true);
    }
    else
    {
        pportbox = new wxRadioBox(panel, wxID_ANY,
            wxT("Parallel Port:"), wxPoint(10, y), wxDefaultSize, 1, wpportopts, 0, wxRA_SPECIFY_COLS,
            wxDefaultValidator, wxT("radioBox"));                             
            my_lisaconfig->parallelp.Replace(_T("profile"),_T("widget"),true);
    }
    y+=ya*2;

    // default to profile for builtin parallel port
    if      (my_lisaconfig->parallel.IsSameAs(_T("Nothing"),false)) pportbox->SetSelection(2);
    else if (my_lisaconfig->parallel.IsSameAs(_T("ADMP"),false))    pportbox->SetSelection(1);
    else                                                            pportbox->SetSelection(0);
    
    m_propath = new wxTextCtrl(panel, wxID_ANY,  my_lisaconfig->parallelp  ,
                               wxPoint(10 * HIDPISCALE, y), wxSize(380 * HIDPISCALE, 30 * HIDPISCALE) , 0);
    b_propath = new wxButton  (panel, ID_PICK_PROFILE, wxT("browse"),  wxPoint(420 * HIDPISCALE, y), wxDefaultSize );


    ya+=ya*2;
    (void) new wxButton( panel, ID_APPLY, wxT("Apply"), applypoint, wxDefaultSize );

    return panel;
}


wxPanel *LisaConfigFrame::CreatePrinterConfigPage(wxNotebook *parent)
{
    wxPanel *panel = new wxPanel(parent);
    int y=10, ya=45;

    (void)new wxStaticText(panel, wxID_ANY, _T("ImageWriter/ADMP DIP Switch 1:"),      
                   wxPoint( 10 * HIDPISCALE,  y), wxSize(400 * HIDPISCALE, 30 * HIDPISCALE));    y+=(ya/2);

    wxString fontopt[]={  wxT("000 American"),      //    ESC Z,^G,^@
                          wxT("001 German"),        //    ESC Z,^C,^@,ESC D,^D,^@
                          wxT("010 American 2"),    //    ESC Z,^E,^@,ESC D,^B,^@
                          wxT("011 French"),        //    ESC Z,^A,^@,ESC D,^F,^@
                          wxT("100 Italian"),       //    ESC Z,^F,^@,ESC D,^A,^@
                          wxT("101 Sweedish"),      //    ESC Z,^B,^@,ESC D,^E,^@
                          wxT("110 British"),       //    ESC Z,^D,^@,ESC D,^C,^@
                          wxT("111 Spanish") };     //    ESC D,^G,^@

    (void)new wxStaticText(panel, wxID_ANY, _T("pins123: Font"),                       
                 wxPoint( 10 * HIDPISCALE,  y), wxSize(300 * HIDPISCALE, 30 * HIDPISCALE));
    dipsw1_123 = new wxChoice(panel, wxID_ANY,  wxPoint(380 * HIDPISCALE, y), wxDefaultSize, 8, fontopt);  y+=ya; y+=ya/2;
    dipsw1_123->SetSelection(my_lisaconfig->iw_dipsw_1 & 7);


       // bit 4-    72 lines (on) 66 lines (off)
       // bit 5-    If on, AND all data with 127 (strip 8th bit)

    wxString bit4opt[]={ wxT("off - 66 lines"), wxT("on - 72 lines") };
    dipsw1_4 = new wxRadioBox(panel, wxID_ANY,
        wxT("pin4: lines"), wxPoint(10 * HIDPISCALE, y), wxDefaultSize, 2, bit4opt, 0, wxRA_SPECIFY_COLS,
        wxDefaultValidator, wxT("radioBox"));                             y+=ya; y+=ya/2;
    dipsw1_4->SetSelection( !!(my_lisaconfig->iw_dipsw_1 & 8) );


    wxString bit5opt[]={ wxT("off - 8 bit"), wxT("on - 7 bit data") };
    dipsw1_5 = new wxRadioBox(panel, wxID_ANY,
        wxT("pin5: bits"), wxPoint(10 * HIDPISCALE, y), wxDefaultSize, 2, bit5opt, 0, wxRA_SPECIFY_COLS,
        wxDefaultValidator, wxT("radioBox"));                             y+=ya; y+=ya/2;
    dipsw1_5->SetSelection( !!(my_lisaconfig->iw_dipsw_1 & 16) );

    wxString bit67opt[]={ wxT("00 Elite Prop."),
                          wxT("01 Elite 12cpi"),
                          wxT("10 Ultracondensed 17cpi"),
                          wxT("11 Pica 10cpi")              };

    (void)new wxStaticText(panel, wxID_ANY, _T("pins67: Pitch"),                       
                   wxPoint( 10 * HIDPISCALE,  y), wxSize(300 * HIDPISCALE, 30 * HIDPISCALE));
    dipsw1_67 = new wxChoice(panel, wxID_ANY, wxPoint(420, y), wxDefaultSize, 4, bit67opt); y+=ya; y+=ya/2;
    dipsw1_67->SetSelection( (my_lisaconfig->iw_dipsw_1>>5) & 3 );

    dipsw1_8 = new wxCheckBox(panel, wxID_ANY, wxT("pin8: Auto LF after CR"), wxPoint(10 * HIDPISCALE,y), wxDefaultSize,wxCHK_2STATE);
    dipsw1_8->SetValue((bool) !!(my_lisaconfig->iw_dipsw_1 & 128) );       y+=ya; y+=ya/2;

    iw_img_box = new wxCheckBox(panel, wxID_ANY, wxT("Print to images"),   wxPoint(10 * HIDPISCALE,y), wxDefaultSize,wxCHK_2STATE);
    iw_img_box->SetValue((bool)(!!my_lisaconfig->iw_png_on) );             y+=ya; y+=ya/2;

    iw_img_path = new wxTextCtrl(panel, wxID_ANY,  my_lisaconfig->iw_png_path , wxPoint(10 * HIDPISCALE, y), 
                          wxSize(400 * HIDPISCALE, 30 * HIDPISCALE) , 0);

    iw_img_path_b = new wxButton  (panel, ID_PICK_IWDIR, wxT("browse"),  wxPoint(420 * HIDPISCALE, y), wxDefaultSize );


    (void) new wxButton( panel, ID_APPLY, wxT("Apply"), applypoint, wxDefaultSize );
    return panel;
}


void LisaConfigFrame::CreateNotebook(wxNotebook *parent)
{
  wxPanel  *panel1 = CreateMainConfigPage( parent );
  wxPanel  *panel2 = CreatePortsConfigPage(parent );
  wxPanel  *panel3 = CreateSlotConfigPage( parent,1);
  wxPanel  *panel4 = CreateSlotConfigPage( parent,2);
  wxPanel  *panel5 = CreateSlotConfigPage( parent,3);
  wxPanel  *panel6 = CreatePrinterConfigPage(parent);

  parent->AddPage( panel1, wxT("config"), false, -1);
  parent->AddPage( panel2, wxT("ports"),  false, -1);
  parent->AddPage( panel3, wxT("slot1"),  false, -1);
  parent->AddPage( panel4, wxT("slot2"),  false, -1);
  parent->AddPage( panel5, wxT("slot3"),  false, -1);
  parent->AddPage( panel6, wxT("print"),  false, -1);

  parent->SetSelection(0);
}



void LisaConfigFrame::OnPickRom(wxCommandEvent& WXUNUSED(event))
{
  wxFileDialog x(NULL, wxT("Open a Lisa Boot ROM") );
  if (x.ShowModal()==wxID_OK) m_rompath->SetValue(x.GetPath());
  //wxString  x=wxFileSelector( wxT("Open a Lisa Boot ROM") );
  //if (x.Len()>3) m_rompath->SetValue(x);
}

void LisaConfigFrame::OnPickDRom(wxCommandEvent& WXUNUSED(event))
{
  wxFileDialog x(NULL, wxT("Open a Lisa Dual Parallel Card ROM") );
  if (x.ShowModal()==wxID_OK) m_dprompath->SetValue(x.GetPath());
}


void LisaConfigFrame::OnPickProFile(wxCommandEvent& WXUNUSED(event))
{
wxFileDialog open(this,                         wxT("Store ProFile drive as:"),
                                                wxEmptyString,
                                                (floppy_iorom==0x88) ? wxT("lisaem-widget.dc42") : wxT("lisaem-profile.dc42"),
                                                wxT("Disk Copy (*.dc42)|*.dc42|All (*.*)|*.*"),
                                                (long int)wxFD_SAVE,wxDefaultPosition);

 if (open.ShowModal()==wxID_OK)                 m_propath->SetValue(open.GetPath());
}

// slot 1

void LisaConfigFrame::OnPickProFile1H(wxCommandEvent& WXUNUSED(event))
{
 wxFileDialog open(NULL, wxT("Create ProFile drive on upper port of Slot 1 as:"),
                                                wxEmptyString,
                                                wxT("lisaem-profile-s1h.dc42"),
                                                wxT("Disk Copy (*.dc42)|*.dc42|All (*.*)|*.*"),
                                                wxFD_SAVE);
 if (open.ShowModal()==wxID_OK)                 m_text_propathh[1]->SetValue(open.GetPath());
}

void LisaConfigFrame::OnPickProFile1L(wxCommandEvent& WXUNUSED(event))
{
 wxFileDialog open(NULL,  wxT("Create ProFile drive on lower port of Slot 1 as:"),
                                                wxEmptyString,
                                                wxT("lisaem-profile-s1l.dc42"),
                                                wxT("Disk Copy (*.dc42)|*.dc42|All (*.*)|*.*"),
                                                wxFD_SAVE);
 if (open.ShowModal()==wxID_OK)                 m_text_propathl[1]->SetValue(open.GetPath());
}


// slot 2

void LisaConfigFrame::OnPickProFile2H(wxCommandEvent& WXUNUSED(event))
{
 wxFileDialog open(NULL,  wxT("Create ProFile drive on upper port of Slot 2 as:"),
                                                wxEmptyString,
                                                wxT("lisaem-profile-s2h.dc42"),
                                                wxT("Disk Copy (*.dc42)|*.dc42|All (*.*)|*.*"),
                                                wxFD_SAVE);

 if (open.ShowModal()==wxID_OK)                 m_text_propathh[2]->SetValue(open.GetPath());

}

void LisaConfigFrame::OnPickProFile2L(wxCommandEvent& WXUNUSED(event))
{
 wxFileDialog open(NULL,  wxT("Create ProFile drive on lower port of Slot 2 as:"),
                                                wxEmptyString,
                                                wxT("lisaem-profile-s2l.dc42"),
                                                wxT("Disk Copy (*.dc42)|*.dc42|All (*.*)|*.*"),
                                                wxFD_SAVE);

 if (open.ShowModal()==wxID_OK)                 m_text_propathl[2]->SetValue(open.GetPath());

}


// slot 3
void LisaConfigFrame::OnPickProFile3H(wxCommandEvent& WXUNUSED(event))
{
 wxFileDialog open(NULL, wxT("Create ProFile drive on upper port of Slot 3 as:"),
                                                wxEmptyString,
                                                wxT("lisaem-profile-s3h.dc42"),
                                                wxT("Disk Copy (*.dc42)|*.dc42|All (*.*)|*.*"),
                                                wxFD_SAVE);

 if (open.ShowModal()==wxID_OK)                 m_text_propathh[3]->SetValue(open.GetPath());

}

void LisaConfigFrame::OnPickProFile3L(wxCommandEvent& WXUNUSED(event))
{
 wxFileDialog open(NULL, wxT("Create ProFile drive on lower port of Slot 3 as:"),
                                                wxEmptyString,
                                                wxT("lisaem-profile-s3l.dc42"),
                                                wxT("Disk Copy (*.dc42)|*.dc42|All (*.*)|*.*"),
                                                wxFD_SAVE);

 if (open.ShowModal()==wxID_OK)                 m_text_propathl[3]->SetValue(open.GetPath());

}


void LisaConfigFrame::OnPickIWDir(wxCommandEvent& WXUNUSED(event))
{
 wxDirDialog dir(NULL, _T("Where should I save the print-out images?") );
 if (dir.ShowModal() == wxID_OK) iw_img_path->SetValue(dir.GetPath());
}


extern void invalidate_configframe(void);

LisaConfigFrame::~LisaConfigFrame()
{
  invalidate_configframe();   // prevent crash on reopening of preferences
}
