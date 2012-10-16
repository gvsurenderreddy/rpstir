/*
 * Created on Nov 7, 2011
 */
package com.bbn.rpki.test.objects;

import java.util.Map;

/**
 * <Enter the description of this type here>
 *
 * @author RTomlinson
 */
public class EE_cert extends Certificate {

  /** */
  public final Object aia;

  /** */
  public final Object crldp;

  /**
   * @param parent
   * @param siaPath
   * @param ipv4
   * @param ipv6
   * @param asList
   * @param myFactory
   * @param serial
   * @param subjkeyfile
   */
  EE_cert(CA_Object parent, int ttl, String bluePrintName, String siaPath, IPRangeList ipv4, IPRangeList ipv6,
          IPRangeList asList) {
    super(parent,
          ttl,
          REPO_PATH + "EE/" + parent.SIA_path,
          bluePrintName,
          siaPath,
          asList,
          ipv4,
          ipv6,
          null,
          "CERTIFICATE",
        "selfsigned=False");
    this.aia   = "rsync://" + Util.removePrefix(parent.path_CA_cert, REPO_PATH);
    this.crldp = "rsync://" + parent.SIA_path + Util.b64encode_wrapper(parent.certificate.ski) + ".crl";
    // Set our SIA based on the hash of our public key, which will be the name
    // of the ROA or Manifest this EE will be signing
    if (bluePrintName.equals("Manifest-EE")) {
      this.sia = "s:rsync://" + parent.SIA_path + Util.b64encode_wrapper(parent.certificate.ski) + ".mft";
      this.ipv4 = IPRangeList.IPV4_INHERIT;
      this.ipv6 = IPRangeList.IPV6_INHERIT;
      this.as_list = IPRangeList.AS_INHERIT;
    } else {
      this.sia = "s:rsync://" + parent.SIA_path + Util.b64encode_wrapper(this.ski) + ".roa";
    }
  }

  /**
   * @see com.bbn.rpki.test.objects.Certificate#getFieldMap(java.util.Map)
   */
  @Override
  public void getFieldMap(Map<String, Object> map) {
    super.getFieldMap(map);
    map.put("aia", aia);
    map.put("crldp", crldp);
  }

}
