#ifndef PW_BROWSER_IODEFINE_H
#define PW_BROWSER_IODEFINE_H




struct st_flash {                                       
                union {                                 
                      unsigned char BYTE;               
                      struct {                          
                             unsigned char P  :1;
      unsigned char E  :1;
      unsigned char PV :1;
      unsigned char EV :1;
      unsigned char PSU:1;
      unsigned char ESU:1;
      unsigned char SWE:1;
      unsigned char    :1;
}      BIT;                
                      }         FLMCR1;                 
                union {                                 
                      unsigned char BYTE;               
                      struct {                          
                             unsigned char FLER:1;      
                             }      BIT;                
                      }         FLMCR2;                 
                union {                                 
                      unsigned char BYTE;               
                      struct {                          
                             unsigned char PDWND:1;     
                             }      BIT;                
                      }         FLPWCR;                 
                union {                                 
                      unsigned char BYTE;               
                      struct {                          
                             unsigned char EB0:1;
      unsigned char EB1:1;
      unsigned char EB2:1;
      unsigned char EB3:1;
      unsigned char EB4:1;
      unsigned char EB5:1;
      unsigned char    :2;
}      BIT;                
                      }         EBR1;                   
                char            wk[7];                  
                union {                                 
                      unsigned char BYTE;               
                      struct {                          
                             unsigned char FLSHE:1;     
                             }      BIT;                
                      }         FENR;                   
};                                                      
struct st_rtc {                                         
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char _025SEIFG:1;
      unsigned char _05SEIFG :1;
      unsigned char _1SEIFG  :1;
      unsigned char MNIFG    :1;
      unsigned char HRIFG    :1;
      unsigned char DYIFG    :1;
      unsigned char WKIFG    :1;
      unsigned char FOIFG    :1;
}      BIT;                  
                    }           RTCFLG;                 
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char SC0:4;
      unsigned char SC1:3;
      unsigned char BSY:1;
}      BIT;                  
                    }           RSECDR;                 
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char MN0:4;
      unsigned char MN1:3;
      unsigned char BSY:1;
}      BIT;                  
                    }           RMINDR;                 
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char HR0:4;
      unsigned char HR1:2;
      unsigned char    :1;
      unsigned char BSY:1;
}      BIT;                  
                    }           RHRDR;                  
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char WK :3;
      unsigned char    :4;
      unsigned char BSY:1;
}      BIT;                  
                    }           RWKDR;                  
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char INT:1;
      unsigned char RST:1;
      unsigned char PM :1;
      unsigned char HR24:1;
      unsigned char RUN:1;
}      BIT;                  
                    }           RTCCR1;                 
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char _025SEIE:1;
      unsigned char _05SEIE :1;
      unsigned char _1SEIE  :1;
      unsigned char MNIE    :1;
      unsigned char HRIE    :1;
      unsigned char DYIE    :1;
      unsigned char WKIE    :1;
      unsigned char FOIE    :1;
}      BIT;                  
                    }           RTCCR2;                 
              char              wk;                     
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char CKSI:4;
      unsigned char CKSO:3;
      unsigned char     :1;
}      BIT;                  
                    }           RTCCSR;                 
};                                                      
struct st_iic2 {                                        
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char CKS :4;
      unsigned char TRS :1;
      unsigned char MST :1;
      unsigned char RCVD:1;
      unsigned char ICE :1;
}      BIT;                 
                     }          ICCR1;                  
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char IICRST:1;
      unsigned char       :1;
      unsigned char SCLO  :1;
      unsigned char SDAOP :1;
      unsigned char SDAO  :1;
      unsigned char SCP   :1;
      unsigned char BBSY  :1;
}      BIT;                 
                     }          ICCR2;                  
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char BC  :3;
      unsigned char BCWP:1;
      unsigned char     :2;
      unsigned char WAIT:1;
      unsigned char MLS :1;
}      BIT;                 
                     }          ICMR;                   
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char ACKBT:1;
      unsigned char ACKBR:1;
      unsigned char ACKE :1;
      unsigned char STIE :1;
      unsigned char NAKIE:1;
      unsigned char RIE  :1;
      unsigned char TEIE :1;
      unsigned char TIE  :1;
}      BIT;                 
                     }          ICIER;                  
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char ADZ  :1;
      unsigned char AAS  :1;
      unsigned char ALOVE:1;
      unsigned char STOP :1;
      unsigned char NACKF:1;
      unsigned char RDRF :1;
      unsigned char TEND :1;
      unsigned char TDRE :1;
}      BIT;                 
                     }          ICSR;                   
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char FS :1;
      unsigned char SVA:7;
}      BIT;                 
                     }          SAR;                    
               unsigned char    ICDRT;                  
               unsigned char    ICDRR;                  
};                                                      
struct st_tb1 {                                         
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char CKS:3;
      unsigned char    :3;
      unsigned char STR:1;
      unsigned char RLD:1;
}      BIT;                  
                    }           TMB1;                   
              unsigned char     TCB1;                   
};                                                      
struct st_comp {                                        
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char CRS :4;
      unsigned char CMLS:1;
      unsigned char CMR :1;
      unsigned char CMIE:1;
      unsigned char CME :1;
}      BIT;                 
                     }          CMCR0;                  
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char CRS :4;
      unsigned char CMLS:1;
      unsigned char CMR :1;
      unsigned char CMIE:1;
      unsigned char CME :1;
}      BIT;                 
                     }          CMCR1;                  
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char CDR0:1;
      unsigned char CDR1:1;
      unsigned char     :2;
      unsigned char CMF0:1;
      unsigned char CMF1:1;
      unsigned char     :2;
}      BIT;                 
                     }          CMDR;                   
};                                                      
struct st_ssu {                                         
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char CSS :2;
      unsigned char SCKS:1;
      unsigned char SOLP:1;
      unsigned char SOL :1;
      unsigned char SOOS:1;
      unsigned char BIDE:1;
      unsigned char MSS :1;
}      BIT;                  
                    }           SSCRH;                  
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char CSOS :1;
      unsigned char SCKOS:1;
      unsigned char SRES :1;
      unsigned char SSUMS:1;
      unsigned char      :1;
}      BIT;                  
                    }           SSCRL;                  
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char CKS :3;
      unsigned char     :2;
      unsigned char CPHS:1;
      unsigned char CPOS:1;
      unsigned char MLS :1;
}      BIT;                  
                    }           SSMR;                   
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char CEIE :1;
      unsigned char RIE  :1;
      unsigned char TIE  :1;
      unsigned char TEIE :1;
      unsigned char      :1;
      unsigned char RSSTP:1;
      unsigned char RE   :1;
      unsigned char TE   :1;
}      BIT;                  
                    }           SSER;                   
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char CE  :1;
      unsigned char RDRF:1;
      unsigned char TDRE:1;
      unsigned char TEND:1;
      unsigned char     :2;
      unsigned char ORER:1;
      unsigned char     :1;
}      BIT;                  
                    }           SSSR;                   
              unsigned char     wk1[4];                 
              unsigned char     SSRDR;                  
              unsigned char     wk2;                    
              unsigned char     SSTDR;                  
};                                                      
struct st_tw {                                          
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char PWMB :1;
      unsigned char PWMC :1;
      unsigned char PWMD :1;
      unsigned char      :1;
      unsigned char BUFEA:1;
      unsigned char BUFEB:1;
      unsigned char      :1;
      unsigned char CTS  :1;
}      BIT;                   
                   }            TMRW;                   
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char TOA :1;
      unsigned char TOB :1;
      unsigned char TOC :1;
      unsigned char TOD :1;
      unsigned char CKS :3;
      unsigned char CCLR:1;
}      BIT;                   
                   }            TCRW;                   
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char IMIEA:1;
      unsigned char IMIEB:1;
      unsigned char IMIEC:1;
      unsigned char IMIED:1;
      unsigned char      :3;
      unsigned char OVIE :1;
}      BIT;                   
                   }            TIERW;                  
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char IMFA:1;
      unsigned char IMFB:1;
      unsigned char IMFC:1;
      unsigned char IMFD:1;
      unsigned char     :3;
      unsigned char OVF :1;
}      BIT;                   
                   }            TSRW;                   
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char IOA:3;
      unsigned char    :1;
      unsigned char IOB:3;
      unsigned char    :1;
}      BIT;                   
                   }            TIOR0;                  
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char IOC:3;
      unsigned char    :1;
      unsigned char IOD:3;
      unsigned char    :1;
}      BIT;                   
                   }            TIOR1;                  
             unsigned int       TCNT;                   
             unsigned int       GRA;                    
             unsigned int       GRB;                    
             unsigned int       GRC;                    
             unsigned int       GRD;                    
};                                                      
struct st_aec {                                         
              unsigned int      ECPWCR;                 
              unsigned int      ECPWDR;                 
              char              wk1[2];                 
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char ECPWME:1;
      unsigned char AIEGS :2;
      unsigned char ALEGS :2;
      unsigned char AHEGS :2;
}      BIT;                  
                    }           AEGSR;                  
              char              wk2;                    
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char PWCK:3;
      unsigned char ACKL:2;
      unsigned char ACKH:2;
}      BIT;                  
                    }           ECCR;                   
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char CRCL:1;
      unsigned char CRCH:1;
      unsigned char CUEL:1;
      unsigned char CUEH:1;
      unsigned char CH2 :1;
      unsigned char     :1;
      unsigned char OVL :1;
      unsigned char OVH :1;
}      BIT;                  
                    }           ECCSR;                  
              union {                                   
                    unsigned int WORD;                  
                    struct {                            
                           unsigned char H;             
                           unsigned char L;             
                           }     BYTE;                  
                    }           EC;                     
};                                                      
struct st_sci3 {                                        
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char SCINV0:1;
      unsigned char SCINV1:1;
      unsigned char       :2;
      unsigned char SPC3  :1;
      unsigned char       :3;
}      BIT;                 
                     }          SPCR;                   
               char             wk1[6];                 
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char CKS :2;
      unsigned char MP  :1;
      unsigned char STOP:1;
      unsigned char PM  :1;
      unsigned char PE  :1;
      unsigned char CHR :1;
      unsigned char COM :1;
}      BIT;                 
                     }          SMR3;                   
               unsigned char    BRR3;                   
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char CKE :2;
      unsigned char TEIE:1;
      unsigned char MPIE:1;
      unsigned char RE  :1;
      unsigned char TE  :1;
      unsigned char RIE :1;
      unsigned char TIE :1;
}      BIT;                 
                     }          SCR3;                   
               unsigned char    TDR3;                   
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char MPBT:1;
      unsigned char MPBR:1;
      unsigned char TEND:1;
      unsigned char PER :1;
      unsigned char FER :1;
      unsigned char OER :1;
      unsigned char RDRF:1;
      unsigned char TDRE:1;
}      BIT;                 
                     }          SSR3;                   
               unsigned char    RDR3;                   
               char             wk2[8];                 
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char ABCS:1;
      unsigned char     :4;
}      BIT;                 
                     }          SEMR;                   
               union {                                  
                     unsigned char BYTE;                
                     struct {                           
                            unsigned char IrCKS:3;
      unsigned char IrE  :1;
}      BIT;                 
                     }          IrCR;                   
};                                                      
struct st_wdt {                                         
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char CKS:4;
      unsigned char    :4;
}      BIT;                  
                    }           TMWD;                   
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char WRST  :1;
      unsigned char B0WI  :1;
      unsigned char WDON  :1;
      unsigned char B2WI  :1;
      unsigned char TCSRWE:1;
      unsigned char B4WI  :1;
      unsigned char TCWE  :1;
      unsigned char B6WI  :1;
}      BIT;                  
                    }           TCSRWD1;                
              union {                                   
                    unsigned char BYTE;                 
                    struct {                            
                           unsigned char IEOVF:1;
      unsigned char B3WI :1;
      unsigned char WTIT :1;
      unsigned char B5WI :1;
      unsigned char OVF  :1;
}      BIT;                  
                    }           TCSRWD2;                
              unsigned char     TCWD;                   
};                                                      
struct st_ad {                                          
             unsigned int       ADRR;                   
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char CH  :4;
      unsigned char CKS :2;
      unsigned char TRGE:1;
      unsigned char     :1;
}      BIT;                   
                   }            AMR;                    
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char LADS:1;
      unsigned char ADSF:1;
}      BIT;                   
                   }            ADSR;                   
};                                                      
struct st_io {                                          
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char B2:1;
      unsigned char B3:1;
      unsigned char B4:1;
      unsigned char   :3;
}      BIT;                   
                   }            PUCR8;                  
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char B0:1;
      unsigned char B1:1;
      unsigned char B2:1;
      unsigned char B3:1;
      unsigned char   :4;
}      BIT;                   
                   }            PUCR9;                  
             unsigned char      TARGET_F088;            
             char               wk1[3];                 
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char B0:1;
      unsigned char B1:1;
      unsigned char B2:1;
      unsigned char B3:1;
      unsigned char   :4;
}      BIT;                   
                   }            PODR9;                  
             char               wk2[3891];              
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char AEVH  :1;
      unsigned char TMOW  :1;
      unsigned char CLKOUT:1;
      unsigned char AEVL  :1;
      unsigned char FTC1  :1;
      unsigned char IRQAEC:1;
      unsigned char       :2;
}      BIT;                   
                   }            PMR1;                   
             char               wk3;                    
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char VCref:1;
      unsigned char      :7;
}      BIT;                   
                   }            PMR3;                   
             char               wk4[7];                 
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char IRQ0    :1;
      unsigned char IRQ1    :1;
      unsigned char         :1;
      unsigned char ADTSTCHG:1;
      unsigned char         :4;
}      BIT;                   
                   }            PMRB;                   
             char               wk5[9];                 
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char B0:1;
      unsigned char B1:1;
      unsigned char B2:1;
      unsigned char   :5;
}      BIT;                   
                   }            PDR1;                   
             char               wk6;                    
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char B0:1;
      unsigned char B1:1;
      unsigned char B2:1;
      unsigned char   :5;
}      BIT;                   
                   }            PDR3;                   
             char               wk7[4];                 
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char B2:1;
      unsigned char B3:1;
      unsigned char B4:1;
      unsigned char   :3;
}      BIT;                   
                   }            PDR8;                   
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char B0:1;
      unsigned char B1:1;
      unsigned char B2:1;
      unsigned char B3:1;
      unsigned char   :4;
}      BIT;                   
                   }            PDR9;                   
             char               wk8;                    
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char B0:1;
      unsigned char B1:1;
      unsigned char B2:1;
      unsigned char B3:1;
      unsigned char B4:1;
      unsigned char B5:1;
      unsigned char   :2;
}      BIT;                   
                   }            PDRB;                   
             char               wk9;                    
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char B0:1;
      unsigned char B1:1;
      unsigned char B2:1;
      unsigned char   :5;
}      BIT;                   
                   }            PUCR1;                  
             union {                                    
                   unsigned char BYTE;                  
                   struct {                             
                          unsigned char B0:1;
      unsigned char B1:1;
      unsigned char B2:1;
      unsigned char   :5;
}      BIT;                   
                   }            PUCR3;                  
             char               wk10[2];                
             unsigned char      PCR1;                   
             char               wk11;                   
             unsigned char      PCR3;                   
             char               wk12[4];                
             unsigned char      PCR8;                   
             unsigned char      PCR9;                   
};                                                      
union un_pfcr {                                         
              unsigned char BYTE;                       
              struct {                                  
                     unsigned char IRQ0S:2;
      unsigned char IRQ1S:2;
      unsigned char SSUS :1;
      unsigned char      :3;
}      BIT;                        
};                                                      
union un_syscr1 {                                       
                unsigned char BYTE;                     
                struct {                                
                       unsigned char MA   :2;
      unsigned char TMA3 :1;
      unsigned char LSON :1;
      unsigned char STS  :3;
      unsigned char SSBY :1;
}      BIT;                      
};                                                      
union un_syscr2 {                                       
                unsigned char BYTE;                     
                struct {                                
                       unsigned char SA   :2;
      unsigned char MSON :1;
      unsigned char DTON :1;
      unsigned char NESEL:1;
      unsigned char      :3;
}      BIT;                      
};                                                      
union un_iegr {                                         
              unsigned char BYTE;                       
              struct {                                  
                     unsigned char IEG0    :1;
      unsigned char IEG1    :1;
      unsigned char         :3;
      unsigned char ADTRGNEG:1;
      unsigned char         :1;
      unsigned char NMIEG   :1;
}      BIT;                        
};                                                      
union un_ienr1 {                                        
               unsigned char BYTE;                      
               struct {                                 
                      unsigned char IEN0  :1;
      unsigned char IEN1  :1;
      unsigned char IENEC2:1;
      unsigned char       :4;
      unsigned char IENRTC:1;
}      BIT;                       
};                                                      
union un_ienr2 {                                        
               unsigned char BYTE;                      
               struct {                                 
                      unsigned char IENEC :1;
      unsigned char       :1;
      unsigned char IENTB1:1;
      unsigned char       :3;
      unsigned char IENAD :1;
      unsigned char       :1;
}      BIT;                       
};                                                      
union un_osccr {                                        
               unsigned char BYTE;                      
               struct {                                 
                      unsigned char OSCF  :1;
      unsigned char       :3;
      unsigned char SUBSEL:1;
      unsigned char RFCUT :1;
      unsigned char SUBSTP:1;
}      BIT;                       
};                                                      
union un_irr1 {                                         
              unsigned char BYTE;                       
              struct {                                  
                     unsigned char IRRI0 :1;
      unsigned char IRRI1 :1;
      unsigned char IRREC2:1;
      unsigned char       :5;
}      BIT;                        
};                                                      
union un_irr2 {                                         
              unsigned char BYTE;                       
              struct {                                  
                     unsigned char IRREC :1;
      unsigned char       :1;
      unsigned char IRRTB1:1;
      unsigned char       :3;
      unsigned char IRRAD :1;
      unsigned char       :1;
}      BIT;                        
};                                                      
union un_ckstpr1 {                                      
                 unsigned char BYTE;                    
                 struct {                               
                        unsigned char RTCCKSTP :1;
      unsigned char FROMCKSTP:1;
      unsigned char TB1CKSTP :1;
      unsigned char          :1;
      unsigned char ADCKSTP  :1;
      unsigned char          :1;
      unsigned char S3CKSTP  :1;
      unsigned char          :1;
}      BIT;                     
};                                                      
union un_ckstpr2 {                                      
                 unsigned char BYTE;                    
                 struct {                               
                        unsigned char COMPCKSTP:1;
      unsigned char WDCKSTP  :1;
      unsigned char AECCKSTP :1;
      unsigned char SSUCKSTP :1;
      unsigned char IICCKSTP :1;
      unsigned char TWCKSTP  :1;
      unsigned char          :1;
}      BIT;                     
};                                                      
extern volatile struct st_flash FLASH; 
extern volatile struct st_rtc RTC; 
extern volatile struct st_iic2 IIC2; 
extern volatile struct st_tb1 TB1; 
extern volatile struct st_comp COMP; 
extern volatile struct st_ssu SSU; 
extern volatile struct st_tw TW; 
extern volatile struct st_aec AEC; 
extern volatile struct st_sci3 SCI3; 
extern volatile struct st_wdt WDT; 
extern volatile struct st_ad AD; 
extern volatile struct st_io IO; 
extern volatile union un_pfcr PFCR; 
extern volatile union un_syscr1 SYSCR1; 
extern volatile union un_syscr2 SYSCR2; 
extern volatile union un_iegr IEGR; 
extern volatile union un_ienr1 IENR1; 
extern volatile union un_ienr2 IENR2; 
extern volatile union un_osccr OSCCR; 
extern volatile union un_irr1 IRR1; 
extern volatile union un_irr2 IRR2; 
extern volatile union un_ckstpr1 CKSTPR1; 
extern volatile union un_ckstpr2 CKSTPR2; 
#define TLB1    TCB1                            

#endif
