# -*- coding: utf-8 -*-
#
# Univention Admin Modules
#  adduser part for the command line interface
#
# Copyright (C) 2004, 2005, 2006 Univention GmbH
#
# http://www.univention.de/
# 
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# Binary versions of this file provided by Univention to you as
# well as other copyrighted, protected or trademarked materials like
# Logos, graphics, fonts, specific documentations and configurations,
# cryptographic keys etc. are subject to a license agreement between
# you and Univention.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import os, sys, getopt, codecs, string, types
import univention.debug
import univention.misc

import univention_baseconfig
import univention.admin.uldap
import univention.admin.config
import univention.admin.modules
import univention.admin.objects
import univention.admin.handlers.users.user
import univention.admin.handlers.groups.group
import univention.admin.handlers.computers.windows

# update choices-lists which are defined in LDAP
univention.admin.syntax.update_choices()

def status(msg):
	out=''
	(utf8_encode,utf8_decode,utf8_reader,utf8_writer)=codecs.lookup('utf-8')
	# univention-adduser is called by Samba when doing "vampire." Since
	# vampire produces a lot of output, and we'd like to print a moderate
	# log, we prepend UNIVENTION to our output. That way we can identify
	# distinguish them from all of Samba's log messages
	#utf8_writer(sys.stderr).write('UNIVENTION %s\n' % msg)
	out='UNIVENTION %s' % msg
	return out

def nscd_invalidate(table):
	if table:
		univention.debug.debug(univention.debug.ADMIN, univention.debug.INFO, 'NSCD: --invalidate %s' % table)
		try:
			univention.misc.close_fd_spawn('/usr/sbin/nscd', ['nscd', '--invalidate', table])
		except:
			univention.debug.debug(univention.debug.ADMIN, univention.debug.INFO, 'NSCD: failed')
			pass
		else:
			univention.debug.debug(univention.debug.ADMIN, univention.debug.INFO, 'NSCD: ok')
			pass

def get_user_object(user, position, lo, co):
	out=''
	usertype='user'
	try:
		# user Account
		userobject=univention.admin.modules.lookup(univention.admin.handlers.users.user, co, lo, scope='domain', base=position.getDn(), filter='(username=%s)' % user, required=1, unique=1)[0]		
	except:
		# machine Account
		for handler in [univention.admin.handlers.computers.windows,univention.admin.handlers.computers.domaincontroller_master,univention.admin.handlers.computers.domaincontroller_slave,univention.admin.handlers.computers.domaincontroller_backup,univention.admin.handlers.computers.managedclient,univention.admin.handlers.computers.memberserver]:
			if not usertype == 'machine': # Account not found in proceeded handlers
				try:
					userobject=univention.admin.modules.lookup(handler, co, lo, scope='domain', base=position.getDn(), filter='(uid=%s)' % user, required=1, unique=1)[0]
					usertype='machine'
				except:
					usertype='unknown'			
	if usertype == 'unknown':
		out='ERROR: account not found, nothing modified'
		return out

	return userobject


def doit(arglist):
	univention.debug.init('/var/log/univention/admin-cmd.log', 1, 1)
	out=[]
	baseConfig=univention_baseconfig.baseConfig()
	baseConfig.load()
	op='add'
	scope='user'
	cmd=os.path.basename(arglist[0])
	if cmd == 'univention-addgroup':
		scope='group'
		op='add'
	elif cmd == 'univention-deluser':
		scope='user'
		op='del'
	elif cmd == 'univention-delgroup':
		scope='group'
		op='del'
	elif cmd == 'univention-addmachine':
		scope='machine'
		op='add'
	elif cmd == 'univention-delmachine':
		scope='machine'
		op='del'
	elif cmd == 'univention-setprimarygroup':
		scope='user'
		op='primarygroup'
	
	opts, args=getopt.getopt(arglist[1:], '', ['status-fd=', 'status-fifo='])
	
	status_fd=None
	for opt, val in opts:
		if opt == '--status-fd':
			status_fd=int(val)
		elif opt == '--status-fifo':
			status_fifo=os.open(val)
	
	co=None
	try:
		lo, position=univention.admin.uldap.getAdminConnection()
	except Exception, e:
		univention.debug.debug(univention.debug.ADMIN, univention.debug.WARN, 'authentication error: %s' % str(e))
		try:
			lo, position=univention.admin.uldap.getMachineConnection()
		except Exception, e2:
			univention.debug.debug(univention.debug.ADMIN, univention.debug.WARN, 'authentication error: %s' % str(e2))
			out.append('authentication error: %s' % str(e))
			out.append('authentication error: %s' % str(e2))
			return out
			pass
	
	for i in range(0, len(args)):
		try:
			args[i]=codecs.utf_8_decode(args[i])[0]
		except:
			args[i]=codecs.latin_1_decode(args[i])[0]
	
	if len(args) == 1:
		if scope == 'machine':
			machine=args[0]
			if machine[-1] == '$':
				machine=machine[0:-1]
			if baseConfig.has_key('samba/defaultcontainer/computer') and baseConfig['samba/defaultcontainer/computer']:
				position.setDn(baseConfig['samba/defaultcontainer/computer'])
			else:
				position.setDn(univention.admin.config.getDefaultContainer(lo, 'computers/windows'))
		elif scope == 'group':
			group=args[0]
			if baseConfig.has_key('samba/defaultcontainer/group') and baseConfig['samba/defaultcontainer/group']:
				position.setDn(baseConfig['samba/defaultcontainer/group'])
			else:
				position.setDn(univention.admin.config.getDefaultContainer(lo, 'groups/group'))
		else:
			user=args[0]
			if baseConfig.has_key('samba/defaultcontainer/user') and baseConfig['samba/defaultcontainer/user']:
				position.setDn(baseConfig['samba/defaultcontainer/user'])
			else:
				position.setDn(univention.admin.config.getDefaultContainer(lo, 'users/user'))
		action=op+scope
			
	elif len(args) == 2:
		user, group=args
		if op == 'del':
			action='deluserfromgroup'
		elif op == 'primarygroup':
			action='setprimarygroup'
		else:
			action='addusertogroup'
	else:
		return out	
	
	if action == 'adduser':
		out.append(status('Adding user %s' % codecs.utf_8_encode(user)[0]))
		object=univention.admin.handlers.users.user.object(co, lo, position=position)
		object.options=['posix', 'person', 'mail']
		object.open()
		object.set_uid_umlauts()
		object['username']=user
		try:
			object['lastname']=codecs.ascii_decode(user)[0]
		except UnicodeEncodeError:
			object['lastname']='unknown'
		a,b=os.popen2('/usr/bin/makepasswd --minchars=8')
		line=b.readline()
		if line[-1] == '\n':
			line=line[0:-1]
		object['password']=line
		object['primaryGroup']=univention.admin.config.getDefaultValue(lo, 'group')
		object.create()
		nscd_invalidate('passwd')
		
	elif action == 'deluser':
		out.append(status('Removing user %s' % codecs.utf_8_encode(user)[0]))
		object=univention.admin.modules.lookup(univention.admin.handlers.users.user, co, lo, scope='domain', base=position.getDomain(), filter='(username=%s)' % user, required=1, unique=1)[0]
		object.remove()
		nscd_invalidate('passwd')
		
	elif action == 'addgroup':
		out.append(status('Adding group %s' % codecs.utf_8_encode(group)[0]))
		object=univention.admin.handlers.groups.group.object(co, lo, position=position)
		object.options=['posix']
		object['name']=group
		object.create()
		nscd_invalidate('group')
		
	elif action == 'delgroup':
		out.append(status('Removing group %s' % codecs.utf_8_encode(group)[0]))
		object=univention.admin.modules.lookup(univention.admin.handlers.groups.group, co, lo, scope='domain', base=position.getDomain(), filter='(name=%s)' % group, required=1, unique=1)[0]
		object.remove()
		nscd_invalidate('group')
		
	elif action == 'addusertogroup':
		ucr_key_samba_bdc_udm_cli_addusertogroup_filter_group='samba/addusertogroup/filter/group'
		if baseConfig.has_key(ucr_key_samba_bdc_udm_cli_addusertogroup_filter_group) and baseConfig[ucr_key_samba_bdc_udm_cli_addusertogroup_filter_group]:
			if group in baseConfig[ucr_key_samba_bdc_udm_cli_addusertogroup_filter_group].split(','):
				out.append(status('addusertogroup: filter protects group "%s"' % (codecs.utf_8_encode(group)[0])))
				return out
		out.append(status('Adding user %s to group %s' % (codecs.utf_8_encode(user)[0], codecs.utf_8_encode(group)[0])))
		groupobject=univention.admin.modules.lookup(univention.admin.handlers.groups.group, co, lo, scope='domain', base=position.getDn(), filter='(name=%s)' % group, required=1, unique=1)[0]
		userobject=get_user_object(user, position, lo, co)
		if isinstance(userobject,types.StringType):
			out.append(userobject)
			return out

		if not userobject.dn in groupobject['users']:
			if groupobject['users'] == ['']:
				groupobject['users'] = [userobject.dn]
			else:
				groupobject['users'].append(userobject.dn)
			groupobject.modify()
			nscd_invalidate('group')
			
	elif action == 'deluserfromgroup':
		out.append(status('Removing user %s from group %s' % (codecs.utf_8_encode(user)[0], codecs.utf_8_encode(group)[0])))
		groupobject=univention.admin.modules.lookup(univention.admin.handlers.groups.group, co, lo, scope='domain', base=position.getDn(), filter='(name=%s)' % group, required=1, unique=1)[0]
	
		userobject=get_user_object(user, position, lo, co)
		if isinstance(userobject,types.StringType):
			out.append(userobject)
			return out

		userobject.open()
		if userobject.dn in groupobject['users'] and not userobject['primaryGroup'] == groupobject.dn:
			groupobject['users'].remove(userobject.dn)
			groupobject.modify()
			nscd_invalidate('group')
	
	elif action == 'addmachine':
		out.append(status('Adding machine %s' % codecs.utf_8_encode(machine)[0]))
		object=univention.admin.handlers.computers.windows.object(co, lo, position=position)
		univention.admin.objects.open(object)
		object.options=[]
		object.set_name_umlauts()
		object['name']=machine
		object['machineAccountGroup']=univention.admin.config.getDefaultValue(lo, 'computerGroup')
		object.create()
		nscd_invalidate('hosts')
		nscd_invalidate('passwd')
		
	elif action == 'delmachine':
		out.append(status('Removing machine %s' % codecs.utf_8_encode(machine)[0]))
		object=univention.admin.modules.lookup(univention.admin.handlers.computers.windows, co, lo, scope='domain', base=position.getDomain(), filter='(name=%s)' % machine, required=1, unique=1)[0]
		object.remove()	
		nscd_invalidate('hosts')
	
	elif action == 'setprimarygroup':
		out.append(status('Set primary group %s for user %s' % (codecs.utf_8_encode(group)[0], codecs.utf_8_encode(user)[0])))
		try:
			groupobject=univention.admin.modules.lookup(univention.admin.handlers.groups.group, co, lo, scope='domain', base=position.getDn(), filter='(name=%s)' % group, required=1, unique=1)[0]
		except:
			out.append('ERROR: group not found, nothing modified')
			return out

		userobject=get_user_object(user, position, lo, co)
		if isinstance(userobject,types.StringType):
			out.append(userobject)
			return out

		if hasattr(userobject,'options'):
			if 'samba' in userobject.options:
				userobject.options.remove('samba')
		userobject.open()
	
		if userobject.has_key('primaryGroup'):
			userobject['primaryGroup']=groupobject.dn
		elif userobject.has_key('machineAccountGroup'):
			userobject['machineAccountGroup']=groupobject.dn
		else:
			out.append('ERROR: unknown group attribute, nothing modified')
			return out
			
		userobject.modify()
	
		if not userobject.dn in groupobject['users']:
			groupobject['users'].append(userobject.dn)
			groupobject.modify()
	
		nscd_invalidate('group')
		nscd_invalidate('passwd')
	return out
