/*
 * Created on Nov 7, 2011
 */
package com.bbn.rpki.test.objects;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import com.bbn.rpki.test.actions.ActionManager;

/**
 * <Enter the description of this type here>
 *
 * @author RTomlinson
 */
public class CA_Object extends Allocator {
  /**
   * Interface to be implemented when iteration over the tree.
   *
   * @author tomlinso
   */
  public interface IterationAction {
    /**
     * Implementation performs the desired action
     * @param caObject
     * @return true to continue the iteration
     */
    boolean performAction(CA_Object caObject);
  }
  /** sia directory path (ends with /) */
  public String SIA_path;
  /** cert common name */
  public String commonName;
  /** the certificate itself */
  public Certificate certificate;
  /** The location (path to) the certificate */
  public String path_CA_cert;
  /** My factory */
  //  public FactoryBase myFactory;

  private int nextChildSN;
  final String bluePrintName;
  private final CA_Object parent;
  /**
   * @return the parent
   */
  public CA_Object getParent() {
    return parent;
  }

  final List<CA_Object> children = new ArrayList<CA_Object>();
  final List<Manifest> manifests = new ArrayList<Manifest>();
  final List<Roa> roas = new ArrayList<Roa>();
  final List<Crl> crl = new ArrayList<Crl>();
  //  private final String manifest_path;
  private final int id;
  private final String nickName;
  private final String subjKeyFile;
  private int manNum = 0;
  private final int ttl;
  private final String serverName;
  private final boolean breakAway;
  /**
   * @param factoryBase
   * @param parent
   * @param id
   * @param subjKeyFile
   */
  public CA_Object(FactoryBase factoryBase, CA_Object parent, int id, String subjKeyFile) {
    this.nextChildSN = 0;
    this.ttl = factoryBase.ttl;
    this.bluePrintName = factoryBase.bluePrintName;
    this.nickName = bluePrintName + "-" + id;
    this.parent = parent;
    this.subjKeyFile = subjKeyFile;
    this.id = id;
    this.serverName = factoryBase.serverName;
    this.breakAway = factoryBase.breakAway;

    if (parent != null) {
      Factory myFactory = (Factory) factoryBase;
      takeIPv4(myFactory.ipv4List, "ini");
      takeIPv6(myFactory.ipv6List, "ini");
      takeAS(myFactory.asList, "ini");
    } else {
      //  trust anchor CA
      IANAFactory myFactory = (IANAFactory) factoryBase;
      this.ipv4Resources = myFactory.ipv4List;
      this.ipv6Resources = myFactory.ipv6List;
      this.asResources = myFactory.asList;
      ActionManager.singleton().recordAllocation(parent, this, "ini", this.ipv4Resources);
      ActionManager.singleton().recordAllocation(parent, this, "ini", this.ipv6Resources);
      ActionManager.singleton().recordAllocation(parent, this, "ini", this.asResources);
    }
    this.ipv4ResourcesFree = new IPRangeList(this.ipv4Resources);
    this.ipv6ResourcesFree = new IPRangeList(this.ipv6Resources);
    this.asResourcesFree = new IPRangeList(this.asResources);

    Certificate certificate = getCertificate();
    // Grab what I need from the certificate
    // Obtain just the SIA path and cut off the r:rsync://
    String[] sia_list = certificate.sia.substring(RSYNC_EXTENSION.length()).split(",");
    this.SIA_path = sia_list[0].substring(0, sia_list[0].length());
    //    this.manifest_path = Util.removePrefix(sia_list[1], RSYNC_EXTENSION);
    this.path_CA_cert = certificate.outputfilename;
    if (parent != null) {
      this.commonName = parent.commonName + "." + this.nickName;
    } else {
      this.commonName = this.nickName;
    }

  }

  /**
   * @return the current certificate for this
   */
  public Certificate getCertificate() {
    if (this.certificate == null || modified) {
      // Initialize our certificate
      if (parent != null) {
        String sia_path = breakAway ? (getServerName() + "/" + nickName + "/") : (parent.SIA_path + nickName + "/");
        String dirPath = REPO_PATH + parent.SIA_path;
        this.certificate = new CA_cert(parent,
                                       getTtl(),
                                       dirPath,
                                       nickName,
                                       sia_path,
                                       this.asResources,
                                       this.ipv4Resources,
                                       this.ipv6Resources,
                                       this.subjKeyFile);
      } else {
        String siaPath = getServerName() + "/" + nickName + "/";
        String dirPath = REPO_PATH + getServerName() + "/";
        this.certificate = new SS_cert(parent, getTtl(), siaPath, nickName, dirPath,
                                       subjKeyFile);
      }
      setModified(false);
    }
    return this.certificate;
  }

  /**
   * @param pairs describe the addresses to take from the parent
   * @param allocationId
   */
  public void takeIPv4(List<Pair> pairs, String allocationId) {
    IPRangeList allocation = parent.subAllocateIPv4(pairs);
    ActionManager.singleton().recordAllocation(parent, this, allocationId, allocation);
    this.ipv4Resources.addAll(allocation);
    setModified(true);
  }

  /**
   * @param pairs describe the addresses to take from the parent
   * @param allocationId
   */
  public void takeIPv6(List<Pair> pairs, String allocationId) {
    IPRangeList allocation = parent.subAllocateIPv6(pairs);
    ActionManager.singleton().recordAllocation(parent, this, allocationId, allocation);
    this.ipv6Resources.addAll(allocation);
    setModified(true);
  }

  /**
   * @param pairs describe the addresses to take from the parent
   * @param allocationId
   */
  public void takeAS(List<Pair> pairs, String allocationId) {
    IPRangeList allocation = parent.subAllocateAS(pairs);
    ActionManager.singleton().recordAllocation(parent, this, allocationId, allocation);
    this.asResources.addAll(allocation);
    setModified(true);
  }

  /**
   * 
   * @param rangeType The type of range being returned
   * @param allocationId
   */
  public void returnAllocation(IPRangeType rangeType, String allocationId) {
    IPRangeList allocation = ActionManager.singleton().findAllocation(parent, this, rangeType, allocationId);
    IPRangeList resources;
    IPRangeList resourcesFree;
    switch(rangeType) {
    default:
      // Should never happen
      return;
    case ipv4:
      resources = this.ipv4Resources;
      resourcesFree = this.ipv4ResourcesFree;
      break;
    case ipv6:
      resources = this.ipv6Resources;
      resourcesFree = this.ipv6ResourcesFree;
      break;
    case as:
      resources = this.asResources;
      resourcesFree = this.asResourcesFree;
      break;
    }
    resources.removeAll(allocation);
    resourcesFree.removeAll(allocation);
    parent.addAll(allocation);
  }

  /**
   * @return the next child serial number
   */
  public int getNextChildSN() {
    return this.nextChildSN++;
  }

  /**
   * Recursively find RPKI objects
   * @param list
   */
  public void appendObjectsToWrite(List<CA_Obj> list) {
    // Create the directory for the objects we're about to store
    String dir_path = REPO_PATH + SIA_path;
    if (!new File(dir_path).isDirectory()) {
      new File(dir_path).mkdirs();
    }
    list.add(getCertificate());
    for (CA_Obj obj : crl) {
      obj.appendObjectsToWrite(list);
      list.add(obj);
    }
    for (Roa obj : roas) {
      obj.appendObjectsToWrite(list);
      list.add(obj);
    }
    for (CA_Object obj : children) {
      obj.appendObjectsToWrite(list);
    }
    for (CA_Obj obj : manifests) {
      obj.appendObjectsToWrite(list);
      list.add(obj);
    }
  }

  /**
   * @param path navigation down to requested descendant
   * @return the requested descendant
   */
  public CA_Object findNode(String...path) {
    return findNode(path, 0);
  }

  private CA_Object findNode(String[] path, int ix) {
    String name = path[ix];
    CA_Object foundChild = null;
    for (CA_Object child : children) {
      if (child.nickName.equals(name)) {
        foundChild = child;
        break;
      }
    }
    if (foundChild != null && ++ix < path.length) {
      foundChild = foundChild.findNode(path, ix);
    }
    return foundChild;
  }

  /**
   * @see java.lang.Object#toString()
   */
  @Override
  public String toString() {
    return commonName;
  }

  /**
   * @return the nickName
   */
  public String getNickname() {
    return nickName;
  }

  /**
   * @return the next manifest number
   */
  public int getNextManifestNumber() {
    return manNum++;
  }

  /**
   * @param action the action to perform on every CA_Object
   */
  public void iterate(IterationAction action) {
    action.performAction(this);
    for (CA_Object child : children) {
      child.iterate(action);
    }
  }

  /**
   * @return the number of children
   */
  public int getChildCount() {
    return children.size();
  }

  /**
   * @param index
   * @return the child at the specified index
   */
  public CA_Object getChild(int index) {
    return children.get(index);
  }

  /**
   * @param child
   * @return the index of the specified child
   */
  public int indexOf(CA_Object child) {
    return children.indexOf(child);
  }

  /**
   * @return the common name
   */
  public String getCommonName() {
    return commonName;
  }

  /**
   * @return the ttl
   */
  public int getTtl() {
    return ttl;
  }

  /**
   * @return the serverName
   */
  public String getServerName() {
    return serverName;
  }
}
