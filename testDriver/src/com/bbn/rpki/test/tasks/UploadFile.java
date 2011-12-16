/*
 * Created on Dec 8, 2011
 */
package com.bbn.rpki.test.tasks;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import com.bbn.rpki.test.objects.Util;

/**
 * Task to upload one file
 *
 * @author tomlinso
 */
public class UploadFile extends Task {

  private final File file;
  private final File repositoryRootDir;
  private final Model model;

  /**
   * @param model 
   * @param repositoryRootDir 
   * @param file
   */
  public UploadFile(Model model, File repositoryRootDir, File file) {
    this.model = model;
    this.file = file;
    this.repositoryRootDir = repositoryRootDir;
  }

  /**
   * @see com.bbn.rpki.test.tasks.Task#run()
   */
  @Override
  public void run() {
    List<String> cmd = new ArrayList<String>();
    cmd.add("scp");
    cmd.add(file.getPath());
    String repository = model.getSCPFileNameArg(repositoryRootDir, file);
    cmd.add(repository);
    Util.exec("UploadFile", false, Util.RPKI_ROOT, null, null, cmd);
    model.uploadedFile(file);
  }

  /**
   * @see com.bbn.rpki.test.tasks.Task#getBreakdownCount()
   */
  @Override
  public int getBreakdownCount() {
    return 0;
  }

  /**
   * @see com.bbn.rpki.test.tasks.Task#getTaskBreakdown(int)
   */
  @Override
  public TaskBreakdown getTaskBreakdown(int n) {
    assert false;
    return null;
  }

  /**
   * @return the file
   */
  public File getFile() {
    return file;
  }

  /**
   * @return the repositoryRootDir
   */
  public File getRepositoryRootDir() {
    return repositoryRootDir;
  }

  /**
   * @see com.bbn.rpki.test.tasks.Task#getLogDetail()
   */
  @Override
  protected String getLogDetail() {
    String repository = model.getSCPFileNameArg(repositoryRootDir, file);
    return String.format("%s to %s", file, repository);
  }
}
