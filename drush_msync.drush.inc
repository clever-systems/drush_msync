<?php

/**
 * Implements hook_drush_command().
 */
function drush_msync_drush_command() {
  $items = array();

  // @todo Check if we must include that file.
  $sql_sync_commands = sqlsync_drush_command();
  $items['msync-sql'] = array(
    'description' => "Does sql-sync for all sites of a multisite. The sites are matched according to their last component (separated by '.').",
  ) + $sql_sync_commands['sql-sync'];

  $core_commands = core_drush_command();
  $items['msync-files'] = array(
    'description' => "Does rsync for all sites of a multisite. The sites are matched according to their last component (separated by '.').",
  ) + $core_commands['core-rsync'];

  return $items;
}

/**
 * Implementation of drush_hook_COMMAND_validate().
 */
function drush_drush_msync_msync_sql_validate($source, $destination) {
  return _drush_msync_validate($source, $destination);
}

/**
 * Implementation of drush_hook_COMMAND_validate().
 */
function drush_drush_msync_msync_files_validate($source, $destination) {
  return _drush_msync_validate($source, $destination);
}

/**
 * @param $source
 * @param $destination
 * @return bool
 */
function _drush_msync_validate($source, $destination) {
  // Rsync path handling, sql-sync alias just has no suffix.
  list($source_alias, $source_suffix) = _drush_msync_split_path($source);
  list($destination_alias, $destination_suffix) = _drush_msync_split_path($destination);

  $source_record = drush_sitealias_get_record($source_alias);
  $destination_record = drush_sitealias_get_record($destination_alias);

  if (!isset($source_record['site-list'])) {
    return drush_set_error('DRUSH_drush_sql_msync_ERROR', dt('Source record must be multiple.'));
  }
  if (!isset($destination_record['site-list'])) {
    return drush_set_error('DRUSH_drush_sql_msync_ERROR', dt('Destination record must be multiple.'));
  }
  return TRUE;
}

/**
 * Implementation of drush_hook_COMMAND().
 */
function drush_drush_msync_msync_sql($source, $destination) {
  _drush_msync_process('sql-sync', $source, $destination);
}

/**
 * Implementation of drush_hook_COMMAND().
 */
function drush_drush_msync_msync_files($source, $destination) {
  _drush_msync_process('rsync', $source, $destination);
}

/**
 * @param $drush_command
 * @param $source
 * @param $destination
 */
function _drush_msync_process($drush_command, $source, $destination) {
  // Rsync path handling, sql-sync alias just has no suffix.
  list($source_alias, $source_suffix) = _drush_msync_split_path($source);
  list($destination_alias, $destination_suffix) = _drush_msync_split_path($destination);

  $source_record = drush_sitealias_get_record($source_alias);
  $destination_record = drush_sitealias_get_record($destination_alias);
  $sources_by_site = _drush_msync_aliases_by_site($source_record['site-list']);
  $destinations_by_site = _drush_msync_aliases_by_site($destination_record['site-list']);

  $common_sources = array_intersect_key($sources_by_site, $destinations_by_site);
  $common_destinations = array_intersect_key($destinations_by_site, $sources_by_site);
  $excess_sources = array_diff_key($sources_by_site, $destinations_by_site);
  $excess_destinations = array_diff_key($destinations_by_site, $sources_by_site);

  if ($excess_sources) {
    drush_log(dt('Excess sources found: @excess', ['@excess' => implode(', ', $excess_sources)]), 'warning');
  }
  if ($excess_destinations) {
    drush_log(dt('Excess destinations found: @excess', ['@excess' => implode(', ', $excess_destinations)]), 'warning');
  }
  if (!$common_sources) {
    drush_log(dt('No common sites found.'), 'warning');
  }
  else {
    drush_log(dt('Syncing: @sites', ['@sites' => implode(', ', array_keys($common_sources))]), 'ok');
  }

  foreach ($common_sources as $site => $current_source) {
    $current_destination = $common_destinations[$site];
    $t_args = [
      '@source' => $current_source,
      '@destination' => $current_destination
    ];
    drush_log(dt('Start: Syncing from @source to @destination', $t_args), 'ok');
    drush_invoke_process('@self', $drush_command, [
      $current_source . $source_suffix,
      $current_destination . $destination_suffix
    ], drush_get_context('cli'));
    drush_log(dt('Done: Syncing from @source to @destination', $t_args), 'success');
  }
}

/**
 * @param $source
 * @return array
 *   Splits 'alias:path' into ['alias',':path']
 *   and 'alias' into ['alias','']
 */
function _drush_msync_split_path($source) {
  $source_parts = preg_split('/(?=:)/', $source, 2);
  $source_parts += [1 => ''];
  return $source_parts;
}

/**
 * @param $site_list
 * @return array
 */
function _drush_msync_aliases_by_site($site_list) {
  $aliases_by_site = [];
  foreach ($site_list as $site_alias_name) {
    $site_name = _drush_msync_extract_alias_site($site_alias_name);
    $aliases_by_site[$site_name] = $site_alias_name;
  }
  return $aliases_by_site;
}

/**
 * @param $site_alias_name
 * @return mixed
 */
function _drush_msync_extract_alias_site($site_alias_name) {
  // Use simple naming scheme to match site.
  // This covers naming schemes like name.site or name:site,
  // and also drush's /drupal/path#site
  // @todo Consider option to get the site name with (much slower) drush status.
  $source_parts = array_reverse(preg_split('/[.:#]/', $site_alias_name));
  $site_name = $source_parts[0];
  return $site_name;
}
