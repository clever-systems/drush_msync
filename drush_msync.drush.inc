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
  $items['msync-sql']['options']['only-sites'] = 'A comma-separeted list of sites to process.';
  $items['msync-sql']['options']['exclude-sites'] = 'A comma-separeted list of sites to exclude.';
  // Same option name as in site-install.
  $items['msync-sql']['options']['db-drop-existing-tables'] = 'If the target DB should be emptied first. Defaults to 1=yes.';

  $core_commands = core_drush_command();
  $items['msync-files'] = array(
    'description' => "Does rsync for all sites of a multisite. The sites are matched according to their last component (separated by '.').",
  ) + $core_commands['core-rsync'];
  $items['msync-sql']['options']['only-sites'] = 'A comma-separeted list of sites to process.';
  $items['msync-files']['options']['exclude-sites'] = 'A comma-separeted list of sites to exclude.';

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
 * @param $source_spec
 * @param $destination_spec
 */
function _drush_msync_process($drush_command, $source_spec, $destination_spec) {
  // Rsync path handling, sql-sync alias just has no suffix.
  list($source_alias, $source_path_suffix) = _drush_msync_split_path($source_spec);
  list($destination_alias, $destination_path_suffix) = _drush_msync_split_path($destination_spec);
  list($sources, $destinations) = _drush_msync_prepare_site_list($source_alias, $destination_alias);

  foreach ($sources as $site => $source) {
    $destination = $destinations[$site];
    $t_args = [
      '@source' => $source,
      '@destination' => $destination
    ];
    drush_log(dt('Start: @source => @destination', $t_args), 'ok');

    if ($drush_command == 'sql-sync' && drush_get_option('db-drop-existing-tables', TRUE)) {
      drush_invoke_process('@self', 'sql-drop', [], drush_get_context('cli'));
    }

    drush_invoke_process('@self', $drush_command, [
      $source . $source_path_suffix,
      $destination . $destination_path_suffix
    ], drush_get_context('cli'));

    drush_log(dt('Done: @source => @destination', $t_args), 'success');
    drush_log(dt('+----------------------------------------------+'), 'ok');
  }
}

/**
 * @param $source_alias
 * @param $destination_alias
 * @return array
 */
function _drush_msync_prepare_site_list($source_alias, $destination_alias) {
  $source_record = drush_sitealias_get_record($source_alias);
  $destination_record = drush_sitealias_get_record($destination_alias);
  $sources_by_site = _drush_msync_aliases_by_site($source_record['site-list']);
  $destinations_by_site = _drush_msync_aliases_by_site($destination_record['site-list']);

  $sources = array_intersect_key($sources_by_site, $destinations_by_site);
  $destinations = array_intersect_key($destinations_by_site, $sources_by_site);
  $excess_sources = array_diff_key($sources_by_site, $destinations_by_site);
  $excess_destinations = array_diff_key($destinations_by_site, $sources_by_site);

  $restrict_sites = drush_get_option_list('only-sites');
  $restrict_sites_as_keys = array_fill_keys($restrict_sites, 1);
  $restricted_sites_to_display = array_keys(array_intersect_key($restrict_sites_as_keys, $sources));
  $sources = array_intersect_key($sources, $restrict_sites_as_keys);

  $excluded_sites = drush_get_option_list('exclude-sites');
  $excluded_sites_as_keys = array_fill_keys($excluded_sites, 1);
  $excluded_sites_to_display = array_keys(array_intersect_key($excluded_sites_as_keys, $sources));
  $sources = array_diff_key($sources, $excluded_sites_as_keys);

  if ($excess_sources) {
    drush_log(dt('Excess sources found: @excess', ['@excess' => implode(', ', $excess_sources)]), 'warning');
  }
  if ($excess_destinations) {
    drush_log(dt('Excess destinations found: @excess', ['@excess' => implode(', ', $excess_destinations)]), 'warning');
  }
  if ($restricted_sites_to_display) {
    drush_log(dt('Restricted to sites: @restricted', ['@restricted' => implode(', ', $restricted_sites_to_display)]), 'warning');
  }
  if ($excluded_sites_to_display) {
    drush_log(dt('Excluded sites: @excluded', ['@excluded' => implode(', ', $excluded_sites_to_display)]), 'warning');
  }
  if (!$sources) {
    drush_log(dt('No sites found to process.'), 'warning');
  }
  else {
    drush_log(dt('Syncing: @sites', ['@sites' => implode(', ', array_keys($sources))]), 'ok');
    if (!drush_confirm('OK?')) {
      $sources = $destinations = [];
    }
  }
  return array($sources, $destinations);
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
