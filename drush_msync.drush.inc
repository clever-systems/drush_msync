<?php

/**
 * Implements hook_drush_command().
 */
function drush_msync_drush_command() {
  $items = array();

  // @todo Check if we must include that file.
  $sql_sync_commands = sqlsync_drush_command();
  $items['drush-msync-sql'] = array(
    'description' => "Does sql-sync in for all sites of a multisite. The sites are matched according to their last component (separated by '.,').",
  ) + $sql_sync_commands['sql-sync'];

  return $items;
}

/**
 * Implementation of drush_hook_COMMAND_validate().
 */
function drush_drush_msync_sql_validate($source, $destination) {
  $source_record = drush_sitealias_get_record($source);
  $destination_record = drush_sitealias_get_record($destination);

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
 *
 * Main command implementation goes here.
 */
function drush_drush_msync_sql($source, $destination) {
  list($sources_by_site, $common_sources, $common_destinations) = _drush_msync_prepare_aliases($source, $destination);

  foreach ($common_sources as $site => $current_source) {
    $current_destination = $common_destinations[$site];
    drush_log(dt('Syncing from @source to @destination', ['@source' => $current_source, '@destination' => $current_destination], 'ok'));
    drush_invoke_process('@none', 'sql-sync', [$current_source, $current_destination], drush_get_context('cli'));
  }

  return $sources_by_site;
}

/**
 * @param $source
 * @param $destination
 * @return array
 */
function _drush_msync_prepare_aliases($source, $destination) {
  $source_record = drush_sitealias_get_record($source);
  $destination_record = drush_sitealias_get_record($destination);
  $sources_by_site = _drush_msync_aliases_by_site($source_record['site-list']);
  $destinations_by_site = _drush_msync_aliases_by_site($destination_record['site-list']);

  $common_sources = array_intersect_key($sources_by_site, $destinations_by_site);
  $common_destinations = array_intersect_key($sources_by_site, $destinations_by_site);
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
    return array($sources_by_site, $common_sources, $common_destinations);
  }
  else {
    drush_log(dt('Syncing: @sites', ['@sites' => implode(', ', array_keys($excess_sources))]), 'ok');
    return array($sources_by_site, $common_sources, $common_destinations);
  }
}

/**
 * @param $site_list
 * @return array
 */
function _drush_msync_aliases_by_site($site_list) {
  $aliases_by_site = [];
  foreach ($site_list as $site_alias_name) {
    $source_parts = implode('.', $site_alias_name);
    $site_name = $source_parts[count($source_parts) - 1];
    $aliases_by_site[$site_name] = $site_alias_name;
  }
  return $aliases_by_site;
}

