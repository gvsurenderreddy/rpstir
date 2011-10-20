/* ***** BEGIN LICENSE BLOCK *****
 * 
 * BBN Rule Editor/Engine for Address and AS Number PKI
 * Verison 1.0
 * 
 * COMMERCIAL COMPUTER SOFTWARE�RESTRICTED RIGHTS (JUNE 1987)
 * US government users are permitted restricted rights as
 * defined in the FAR.  
 *
 * This software is distributed on an "AS IS" basis, WITHOUT
 * WARRANTY OF ANY KIND, either express or implied.
 *
 * Copyright (C) Raytheon BBN Technologies Corp. 2007.  All Rights Reserved.
 *
 * Contributor(s):  Charlie Gardiner
 *
 * ***** END LICENSE BLOCK ***** */
package Algorithms;
import asn.*;
public class Different_Parms extends AsnSequence
    {
    public Kea_Parms keaparms = new Kea_Parms();
    public Dss_Parms dssparms = new Dss_Parms();
    public Different_Parms()
        {
        _tag = AsnStatic.ASN_SEQUENCE;
        _type = (short)AsnStatic.ASN_SEQUENCE;
        _setup((AsnObj)null, keaparms, (short)0, (int)0x0);
        _setup(keaparms, dssparms, (short)0, (int)0x0);
        }
    public Different_Parms set(Different_Parms frobj)
        {
        ((AsnObj)this).set(frobj);
	return this;
	}
    }