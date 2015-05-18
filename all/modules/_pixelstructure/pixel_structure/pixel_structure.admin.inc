<?php
/**
 * Created by PhpStorm.
 * User: ralph
 * Date: 27.04.15
 * Time: 18:01
 */

/**
 * A bit flag used to let us know if a node type is in the database.
 */
define('NODE_TYPE_IN_DATABASE', 0x01);

/**
 * A bit flag used to let us know if a node type is in code and not imported yet.
 */
define('NODE_TYPE_IN_CODE', 0x02);



/**
 * Page callback to list all node types in code and db in a table. THe node types can be managed on this page.
 *
 * @return array    The render array of the table.
 */
function pixel_structure_node_type_list() {
  $header = array(
    array('data' => t('Node types')),
    array('data' => t('Last import/export')),
    array('data' => t('Code created')),
    array('data' => t('Status')),
    array('data' => t('Operations'), 'colspan' => 4),
  );
  $rows = array();

  // create node type rows
  $node_types = _pixel_structure_node_types();
  foreach ($node_types as $key => $node) {
    $status = '';
    $import = '';
    $export = '';
    $delete = '';
    $last_import_export = '';
    $code_exported = '';
    // Determine storage
    switch ($node['storage']) {
      case NODE_TYPE_IN_DATABASE | NODE_TYPE_IN_CODE:
        if ($node['last_import_export'] == $node['code_exported']) {
          // node is up to date
          $status = t('<strong>Up-to-date</strong>');
          $export = l(t('Export'), 'admin/structure/pixel_structure/manage/'. $key .'/export');
        } else {
          if ($node['last_import_export'] > $node['code_exported']) {
            $status = t('<strong>DB is newer</strong>');
          } else {
            $status = t('<strong>Code is newer</strong>');
          }
          // let user decide what to update
          $import = l(t('Update DB'), 'admin/structure/pixel_structure/manage/'. $key .'/import');
          $export = l(t('Update Code'), 'admin/structure/pixel_structure/manage/'. $key .'/export');
        }
        $delete = l(t('Delete'), 'admin/structure/pixel_structure/manage/'. $key .'/delete');
        $last_import_export = ($node['last_import_export'] == -1) ? 'never' : date('d-M-Y H:i:s', $node['last_import_export']);
        $code_exported = ($node['code_exported'] == 0) ? 'undefined' : date('d-M-Y H:i:s', $node['code_exported']);
        break;
      case NODE_TYPE_IN_DATABASE:
        $status = t('<strong>DB only</strong>');
        $export = l(t('Export'), 'admin/structure/pixel_structure/manage/'. $key .'/export');
        $delete = l(t('Delete'), 'admin/structure/pixel_structure/manage/'. $key .'/delete');
        $last_import_export = ($node['last_import_export'] == -1) ? 'never' : date('d-M-Y H:i:s', $node['last_import_export']);
        break;
      case NODE_TYPE_IN_CODE:
        $status = t('<strong>Code only</strong>');
        $import = l(t('Import'), 'admin/structure/pixel_structure/manage/'. $key .'/import');
        $code_exported = ($node['code_exported'] == 0) ? 'undefined' : date('d-M-Y H:i:s', $node['code_exported']);
        break;
    }

    $tablerow = array(
      array('data' => check_plain($node['title'])),
      array('data' => $last_import_export),
      array('data' => $code_exported),
      array('data' => $status),
      array('data' => $import),
      array('data' => $export),
      array('data' => $delete),
      //array('data' => l(t('Clone'), 'admin/structure/pixel_structure/manage/'. $key .'/clone')),
    );
    $rows[] = $tablerow;
  }

  if (empty($rows)) {
    $rows[] = array(array('data' => t('No node types available.'), 'colspan' => 8));
  }

  $build = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#attributes' => array('id' => 'pixel-structure'),
  );
  return $build;
}


/**
 * Imports the node type from code.
 *
 * @param $node_type  string    The node type machine name to be imported.
 */
function pixel_structure_node_type_import($node_type) {
  // read import data
  $nt_data = pixel_structure_node_type_import_data($node_type);
  if ($nt_data) {
    // import content type data
    bundle_copy_import_submit(array(), $nt_data);

    // update time stamp (and code)
    $code_exported = $nt_data['code_exported'];
    if ($code_exported == 0) {
      // update export file with time stamp
      $code_exported = time();
      $nt_data['code_exported'] = $code_exported;
      pixel_structure_node_type_export_data($node_type, $nt_data);

    } else {
      // update db time stamp
      variable_set('pixel_structure_last_import_export_' . $node_type, $code_exported);
    }


  } else {
    // import data couldn't be read
    drupal_set_message(t('%node data could not be imported. No data available!', array('%node' => $nt_data['title'])), 'error');
  }

  // return node type list
  return pixel_structure_node_type_list();
}

/**
 * Exports the node type to code. The file name is "node_type.node" and is located in the nodes folder of this module.
 *
 * @param $node_type  string    The node type machine name to be imported.
 */
function pixel_structure_node_type_export_form($form, &$form_state, $node_type) {
  // skip first step (bundle selection), only export given node type
  if (!isset($form_state['step'])) {
    $form_state['step'] = 2;
    $form_state['node_type'] = $node_type;
    $form_state['page_values'][1] = array(
      'bundles' => array($node_type => $node_type),
    );
    // create form for step 2
    $form = bundle_copy_export($form, $form_state);

    // adapt cancel button for step 2
    $form['actions']['cancel'] = array(
      '#markup' => l(t('Cancel'), 'admin/structure/pixel_structure'),
    );

  }

  return $form;
}

/**
 * Submit callback: export data.
 */
function pixel_structure_node_type_export_form_submit($form, &$form_state) {
  // update form state for multi-step form
  bundle_copy_export_submit($form, $form_state);

  // overwrite third step: export node type directly to file
  if (isset($form_state['step']) && $form_state['step'] == 3) {

    // check if node type is available
    if (isset($form_state['node_type'])) {
      // create export data
      $node_type = $form_state['node_type'];
      $data = _bundle_copy_export_data('node', $form_state['page_values']);
      $nt_data = array(
        'title' => node_type_get_names()[$node_type],
        'code_exported' => time(),
        'data' => $data,
      );

      // export data to file
      pixel_structure_node_type_export_data($node_type, $nt_data);

    } else {
      // node type not defined
      drupal_set_message(t('Node type not available. No export performed!'), 'error');

    }

    // redirect to pixel structure main config page
    $form_state['rebuild'] = FALSE;
    $form_state['redirect'] = 'admin/structure/pixel_structure';
  }

}

/**
 * Confirm form announcing the deletion of a node type.
 */
function pixel_structure_node_type_delete_confirm($form, &$form_state, $node_type) {
  module_load_include('inc', 'node', 'content_types');
  $type = node_type_load($node_type);
  $confirm = node_type_delete_confirm($form, $form_state, $type);
  $confirm['actions']['cancel']['#href'] = 'admin/structure/pixel_structure';
  $confirm['actions']['cancel']['#options']['path'] = 'admin/structure/pixel_structure';
  return $confirm;
}

/**
 * Deletes an existing node type.
 *
 * @see pixel_structure_node_type_delete_form()
 */
function pixel_structure_node_type_delete_confirm_submit($form, &$form_state) {
  node_type_delete_confirm_submit($form, $form_state);

  // redirect to pixel structure main config page
  $form_state['redirect'] = 'admin/structure/pixel_structure';
}

/**
 * Clones an existing node type.
 *
 * @param $node_type  string    The node type machine name to be cloned.
 */
function pixel_structure_node_type_clone($node_type) {
  // TODO: clone the node type

  // return node type list
  return pixel_structure_node_type_list();
}

/**
 * Returns an array of all available content types defined in the pixel structure (code and db).
 *
 * @return  array   An associative array of all existing (in db) and importable content types (stored in code files).
 *                  The keys are the machine names of each content type and the value is an associative array
 *                  with the node machine name as key and the import data as value.
 */
function _pixel_structure_node_types() {
  // get all node types in the database
  $nodes = array();
  foreach (node_type_get_names() as $key => $title) {
    $last_import_export = variable_get('pixel_structure_last_import_export_' . $key, -1);
    $nodes[$key] = array('title' => $title,
                         'storage' => NODE_TYPE_IN_DATABASE,
                         'last_import_export' => $last_import_export);
  }

  // scan nodes directory for nodes in code
  $files = file_scan_directory(drupal_get_path('module', 'pixel_structure') . '/nodes', '#\.node$#');
  foreach ($files as $filepath => $file) {
    // import node type code
    require $filepath;

    // Add data to list of nodes to provide.
    if (isset($data) && is_array($data)) {
      // get export timestamp in file
      $code_exported = isset($export_time) ? $export_time : 0;

      // Create array entries.
      foreach ($data['bundles'] as $key => $bundle) {
        $title = $key; // fallback for title is machine name
        if (is_object($bundle)) {
          $title = $bundle->name;
        }
        elseif (is_array($bundle)) {
          $title = $bundle['name'];
        }

        if (array_key_exists($key, $nodes)) {
          // node type already in db
          $nodes[$key]['storage'] |= NODE_TYPE_IN_CODE;
          $nodes[$key]['code_exported'] = $code_exported;

        } else {
          // node type only in code
          $nodes[$key] = array('title' => $title,
                               'storage' => NODE_TYPE_IN_CODE,
                               'code_exported' => $code_exported);
        }
      }
    }
  }

  return $nodes;
}

