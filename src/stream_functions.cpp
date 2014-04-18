#include "ccextractor.h"
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

void detect_stream_type (void)
{
    stream_mode=CCX_SM_ELEMENTARY_OR_NOT_FOUND; // Not found
	startbytes_avail = (int) buffered_read_opt (startbytes,STARTBYTESLENGTH);
    
    if( startbytes_avail == -1)
        fatal (EXIT_READ_ERROR, "Error reading input file!\n");

    if (startbytes_avail>=4)
    {
        // Check for ASF magic bytes
        if (startbytes[0]==0x30 &&
            startbytes[1]==0x26 &&
            startbytes[2]==0xb2 &&
            startbytes[3]==0x75)
            stream_mode=CCX_SM_ASF; 
    }
    if(startbytes_avail>=4)
    {
        if(startbytes[0]==0xb7 &&
            startbytes[1]==0xd8 &&
            startbytes[2]==0x00 &&
            startbytes[3]==0x20)
            stream_mode=CCX_SM_WTV;
    }
	if (stream_mode==CCX_SM_ELEMENTARY_OR_NOT_FOUND && startbytes_avail>=6)
	{
		// Check for hexadecimal dump generated by wtvccdump
		// ; CCHD
        if (startbytes[0]==';' &&
            startbytes[1]==' ' &&
            startbytes[2]=='C' &&
            startbytes[3]=='C' &&
            startbytes[4]=='H' &&
			startbytes[5]=='D')             
            stream_mode= CCX_SM_HEX_DUMP;
	}

    if (stream_mode==CCX_SM_ELEMENTARY_OR_NOT_FOUND && startbytes_avail>=11)
    {
        // Check for CCExtractor magic bytes
        if (startbytes[0]==0xCC &&
            startbytes[1]==0xCC &&
            startbytes[2]==0xED &&
            startbytes[8]==0 &&
            startbytes[9]==0 &&
            startbytes[10]==0)
            stream_mode=CCX_SM_RCWT;
    }
    if (stream_mode==CCX_SM_ELEMENTARY_OR_NOT_FOUND) // Still not found
    {
        if (startbytes_avail > 188*8) // Otherwise, assume no TS
        {
            // First check for TS
            for (unsigned i=0; i<188;i++)
            {
                if (startbytes[i]==0x47 && startbytes[i+188]==0x47 && 
                    startbytes[i+188*2]==0x47 && startbytes[i+188*3]==0x47 &&
					startbytes[i+188*4]==0x47 && startbytes[i+188*5]==0x47 &&
					startbytes[i+188*6]==0x47 && startbytes[i+188*7]==0x47 
				)
                {
                    // Eight sync bytes, that's good enough 
                    startbytes_pos=i;
                    stream_mode=CCX_SM_TRANSPORT;
                    break;
                }                           
            }
            // Now check for PS (Needs PACK header)
            for (unsigned i=0;
                 i < unsigned(startbytes_avail<50000?startbytes_avail-3:49997);
                 i++)
            {
                if (startbytes[i]==0x00 && startbytes[i+1]==0x00 && 
                    startbytes[i+2]==0x01 && startbytes[i+3]==0xBA)
                {
                    // If we find a PACK header it is not an ES
                    startbytes_pos=i;
                    stream_mode=CCX_SM_PROGRAM;
                    break;
                }                           
            }
            // TiVo is also a PS
            if (startbytes[0]=='T' && startbytes[1]=='i' && 
                startbytes[2]=='V' && startbytes[3]=='o')
            {
                // The TiVo header is longer, but the PS loop will find the beginning
                startbytes_pos=187;
                stream_mode=CCX_SM_PROGRAM;
                strangeheader=1; // Avoid message about unrecognized header
            }                           
        }
        else
        {
            startbytes_pos=0;
            stream_mode=CCX_SM_ELEMENTARY_OR_NOT_FOUND;
        }
    }
	if (stream_mode==CCX_SM_ELEMENTARY_OR_NOT_FOUND && startbytes_avail>=4) // Still not found
	{
		// Try for MP4 by looking for box signatures - this should happen very 
		// early in the file according to specs
		for (int i=0;i<startbytes_avail-3;i++)
		{
			// Look for the a box of type 'file'
			if (
			   (startbytes[i]=='f' && startbytes[i+1]=='t' && 
               startbytes[i+2]=='y' && startbytes[i+3]=='p') 
			   ||
			   (startbytes[i]=='m' && startbytes[i+1]=='o' && 
               startbytes[i+2]=='o' && startbytes[i+3]=='v') 
			   ||
			   (startbytes[i]=='m' && startbytes[i+1]=='d' && 
               startbytes[i+2]=='a' && startbytes[i+3]=='t') 
			   ||
			   (startbytes[i]=='f' && startbytes[i+1]=='e' && 
               startbytes[i+2]=='e' && startbytes[i+3]=='e') 
			   ||
			   (startbytes[i]=='s' && startbytes[i+1]=='k' && 
               startbytes[i+2]=='i' && startbytes[i+3]=='p') 
			   ||
			   (startbytes[i]=='u' && startbytes[i+1]=='d' && 
               startbytes[i+2]=='t' && startbytes[i+3]=='a') 
			   ||
			   (startbytes[i]=='m' && startbytes[i+1]=='e' && 
               startbytes[i+2]=='t' && startbytes[i+3]=='a') 
			   ||
			   (startbytes[i]=='v' && startbytes[i+1]=='o' && 
               startbytes[i+2]=='i' && startbytes[i+3]=='d') 
			   )
			{
				stream_mode=CCX_SM_MP4;
				break;
			}
		}
	}
   // Don't use STARTBYTESLENGTH. It might be longer than the file length!
	return_to_buffer (startbytes, startbytes_avail);
}


int detect_myth( void )
{
    int vbi_blocks=0;
    // VBI data? if yes, use myth loop
    // STARTBTYTESLENGTH is 1MB, if the file is shorter we will never detect
    // it as a mythTV file
    if (startbytes_avail==STARTBYTESLENGTH)
    {
        unsigned char uc[3];
        memcpy (uc,startbytes,3);
        for (int i=3;i<startbytes_avail;i++)
        {
            if ((uc[0]=='t') && (uc[1]=='v') && (uc[2] == '0') ||
                (uc[0]=='T') && (uc[1]=='V') && (uc[2] == '0'))
                vbi_blocks++;
            uc[0]=uc[1];
            uc[1]=uc[2];
            uc[2]=startbytes[i];
        }							
    }
    if (vbi_blocks>10) // Too much coincidence
        return 1;

    return 0;
}

/* Read and evaluate the current video PES header. The function returns
 * the length of the payload if successful, otherwise -1 is returned
 * indicating a premature end of file / too small buffer.
 * If sbuflen is
 *    0 .. Read from file into nextheader
 *    >0 .. Use data in nextheader with the length of sbuflen
 */
int read_video_pes_header (unsigned char *nextheader, int *headerlength, int sbuflen)
{
    // Read the next video PES
    // ((nextheader[3]&0xf0)==0xe0)
    unsigned peslen=nextheader[4]<<8 | nextheader[5];
    unsigned payloadlength = 0; // Length of packet data bytes
	static LLONG current_pts_33=0; // Last PTS from the header, without rollover bits

    if ( !sbuflen )
    {
        // Extension present, get it
        buffered_read (nextheader+6,3);
        past=past+result;
        if (result!=3) {
            // Consider this the end of the show.
            return -1;
        }
    }
    else
    {
	// We need at least 9 bytes to continue
	if( sbuflen < 9 )
	    return -1;
    }
    *headerlength = 6+3;

    unsigned hskip=0;

    // Assume header[8] is right, but double check
    if ( !sbuflen )
    {
        if (nextheader[8] > 0) {
            buffered_read (nextheader+9,nextheader[8]);
            past=past+result;
            if (result!=nextheader[8]) {
                return -1;
            }
        }
    }
    else
    {
	// See if the buffer is big enough
	if( sbuflen < *headerlength + (int)nextheader[8] )
	    return -1;
    }
    *headerlength += (int) nextheader[8];
    int falsepes = 0;
    int pesext = 0;

    // Avoid false positives, check --- not really needed
    if ( (nextheader[7]&0xC0) == 0x80 ) {
        // PTS only
        hskip += 5;
        if( (nextheader[9]&0xF1) != 0x21 || (nextheader[11]&0x01) != 0x01
            || (nextheader[13]&0x01) != 0x01 ) {
            falsepes = 1;
            mprint("False PTS\n");
        }
    } else if ( (nextheader[7]&0xC0) == 0xC0 ) {
        // PTS and DTS
        hskip += 10;
        if( (nextheader[9]&0xF1) != 0x31 || (nextheader[11]&0x01) != 0x01
            || (nextheader[13]&0x01) != 0x01
            || (nextheader[14]&0xF1) != 0x11 || (nextheader[16]&0x01) != 0x01
            || (nextheader[18]&0x01) != 0x01 ) {
            falsepes = 1;
            mprint("False PTS/DTS\n");
        }
    } else if ( (nextheader[7]&0xC0) == 0x40 ) {
        // Forbidden
        falsepes = 1;
        mprint("False PTS/DTS flag\n");
    }
    if ( !falsepes && nextheader[7]&0x20 ) { // ESCR
        if ((nextheader[9+hskip]&0xC4) != 0x04 || !(nextheader[11+hskip]&0x04)
            || !(nextheader[13+hskip]&0x04) || !(nextheader[14+hskip]&0x01) ) {
            falsepes = 1;
            mprint("False ESCR\n");
        }
        hskip += 6;
    }
    if ( !falsepes && nextheader[7]&0x10 ) { // ES
        if ( !(nextheader[9+hskip]&0x80) || !(nextheader[11+hskip]&0x01) ) {
            mprint("False ES\n");
            falsepes = 1;
        }
        hskip += 3;
    }
    if ( !falsepes && nextheader[7]&0x04) { // add copy info
        if ( !(nextheader[9+hskip]&0x80) ) {
            mprint("False add copy info\n");
            falsepes = 1;
        }
        hskip += 1;
    }
    if ( !falsepes && nextheader[7]&0x02) { // PES CRC
        hskip += 2;
    }
    if ( !falsepes && nextheader[7]&0x01) { // PES extension
        if ( (nextheader[9+hskip]&0x0E)!=0x0E ) {
            mprint("False PES ext\n");
            falsepes = 1;
        }
        hskip += 1;
        pesext = 1;
    }

    if ( !falsepes ) {
        hskip = nextheader[8];
    }

    if ( !falsepes && nextheader[7]&0x80 ) {
        // Read PTS from byte 9,10,11,12,13
		
		LLONG bits_9 = ((LLONG) nextheader[9] & 0x0E)<<29; // PTS 32..30 - Must be LLONG to prevent overflow 
		unsigned bits_10 = nextheader[10] << 22; // PTS 29..22
		unsigned bits_11 = (nextheader[11] & 0xFE) << 14; // PTS 21..15
		unsigned bits_12 = nextheader[12] << 7; // PTS 14-7
		unsigned bits_13 = nextheader[13] >> 1; // PTS 6-0
		
		if (pts_set) // Otherwise can't check for rollovers yet
		{
			if (!bits_9 && ((current_pts_33>>30)&7)==7) // PTS about to rollover
				rollover_bits++;
			if ((bits_9>>30)==7 && ((current_pts_33>>30)&7)==0) // PTS rollback? Rare and if happens it would mean out of order frames
				rollover_bits--;
		}


		current_pts_33 = bits_9 | bits_10 | bits_11 | bits_12 | bits_13;
		current_pts = (LLONG) rollover_bits<<33 | current_pts_33;

        if (pts_set==0)
            pts_set=1;

        dbg_print(CCX_DMT_VERBOSE, "Set PTS: %s (%u)\n",
                   print_mstime((current_pts)/(MPEG_CLOCK_FREQ/1000)),
                   unsigned(current_pts) );
        /* The user data holding the captions seems to come between GOP and
         * the first frame. The sync PTS (sync_pts) (set at picture 0)
         * corresponds to the first frame of the current GOP. */
    }

    // This might happen in PES packets in TS stream. It seems that when the
    // packet length is unkown it is set to 0.
    if (peslen+6 >= hskip+9)
    {
	payloadlength = peslen - (hskip + 3); // for [6], [7] and [8]
    }

    // Use a save default if this is not a real PES header
    if (falsepes) {
        mprint("False PES header\n");
    }

    dbg_print(CCX_DMT_VERBOSE, "PES header length: %d (%d verified)  Data length: %d\n",
               *headerlength, hskip+9, payloadlength);

    return payloadlength;
}
