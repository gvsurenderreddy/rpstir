/*
 * Created on Feb 2, 2012
 */
package com.bbn.rpki.test.actions.ui;

import java.awt.BorderLayout;
import java.awt.Component;
import java.awt.Dimension;
import java.awt.event.ActionEvent;
import java.lang.reflect.Constructor;

import javax.swing.Action;
import javax.swing.JButton;
import javax.swing.JComboBox;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JSplitPane;
import javax.swing.tree.TreePath;

import com.bbn.rpki.test.actions.AbstractAction;
import com.bbn.rpki.test.actions.AllocateAction;
import com.bbn.rpki.test.actions.ChooseCacheCheckTask;
import com.bbn.rpki.test.actions.Epoch;
import com.bbn.rpki.test.actions.ui.ActionTree.SelectionListener;
import com.bbn.rpki.test.tasks.Model;

/**
 * <Enter the description of this type here>
 *
 * @author tomlinso
 */
public class ActionsEditor implements SelectionListener {
  private final JSplitPane splitPane;
  private final ActionTree actionTree;
  private final JScrollPane treePane;
  private final ActionDetail actionDetail;
  private TreePath selectedPath = null;

  private final Action addAction = new javax.swing.AbstractAction("Add Action") {
    @Override
    public void actionPerformed(ActionEvent e) {
      Class<?>[] classes = {
          AllocateAction.class,
          ChooseCacheCheckTask.class
      };
      String[] names = new String[classes.length];
      for (int i = 0; i < names.length; i++) {
        names[i] = classes[i].getSimpleName();
      }
      JComboBox box = new JComboBox(names);
      int option = JOptionPane.showConfirmDialog(treePane, box, "Select a Type of Action", JOptionPane.OK_CANCEL_OPTION, JOptionPane.QUESTION_MESSAGE);
      if (option == JOptionPane.OK_OPTION) {
        int index = box.getSelectedIndex();
        if (index >= 0) {
          Class<?> chosenClass = classes[index];
          try {
            Constructor<?> constructor = chosenClass.getConstructor(Model.class);
            AbstractAction addedAction = (AbstractAction) constructor.newInstance(model);
            model.addAction(addedAction);
          } catch (Exception ex) {
            return;
          }
        }
      }
    }
  };

  private final Action deleteAction = new javax.swing.AbstractAction("Delete") {
    @Override
    public void actionPerformed(ActionEvent e) {
      assert selectedPath != null;
      int pathCount = selectedPath.getPathCount();
      if (pathCount == 3) {
        Epoch epoch = (Epoch) selectedPath.getPathComponent(2);
        AbstractAction action = epoch.getAction();
        model.removeAction(action);
      }
    }
  };

  private final Action expandAction = new javax.swing.AbstractAction("Expand") {
    @Override
    public void actionPerformed(ActionEvent e) {
      actionTree.expand();
    }
  };

  private final Action collapseAction = new javax.swing.AbstractAction("Collapse") {
    @Override
    public void actionPerformed(ActionEvent e) {
      actionTree.collapse();
    }
  };
  private final Model model;

  /**
   * @param model
   */
  public ActionsEditor(Model model) {
    this.model = model;
    actionTree = new ActionTree(model, this);
    actionDetail = new ActionDetail(model);
    treePane = new JScrollPane(actionTree.getComponent());
    JPanel treePanel = new JPanel(new BorderLayout());
    treePanel.add(treePane);
    JPanel treeButtons = new JPanel();
    JButton[] buttons = {
        new JButton(addAction),
        new JButton(deleteAction),
        new JButton(expandAction),
        new JButton(collapseAction)
    };
    for (JButton jButton : buttons) {
      treeButtons.add(jButton);
    }
    treePanel.add(treeButtons, BorderLayout.SOUTH);
    splitPane = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, treePanel, actionDetail.getComponent());
    splitPane.setDividerLocation(0.5);
    splitPane.setResizeWeight(0.0);
    splitPane.setPreferredSize(new Dimension(1024, 768));
    selectionChanged(null);
  }

  /**
   * @return the GUI component
   */
  public Component getComponent() {
    return splitPane;
  }

  private void changeAction(Action action, String name) {
    if (name == null) {
      action.setEnabled(false);
    } else {
      action.putValue(Action.NAME, name);
      action.setEnabled(true);
    }
  }

  /**
   * @see com.bbn.rpki.test.actions.ui.ActionTree.SelectionListener#selectionChanged(javax.swing.tree.TreePath)
   */
  @Override
  public void selectionChanged(TreePath newPath) {
    selectedPath = newPath;
    int pathCount;
    if (newPath != null) {
      pathCount = newPath.getPathCount();
    } else {
      pathCount = 0;
    }
    if (pathCount > 2) {
      Epoch epoch = (Epoch) newPath.getPathComponent(2);
      AbstractAction action = epoch.getAction();
      actionDetail.setAction(action, epoch);
      changeAction(deleteAction, "Delete Action");
    } else {
      actionDetail.setAction(null, null);
      changeAction(deleteAction, null);
    }
  }
}