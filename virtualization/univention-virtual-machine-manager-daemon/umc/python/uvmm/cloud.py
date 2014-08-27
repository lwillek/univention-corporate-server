# -*- coding: utf-8 -*-
#
# Univention Management Console
#  UVMM cloud commands
#
# Copyright 2014 Univention GmbH
#
# http://www.univention.de/
#
# All rights reserved.
#
# The source code of this program is made available
# under the terms of the GNU Affero General Public License version 3
# (GNU AGPL V3) as published by the Free Software Foundation.
#
# Binary versions of this program provided by Univention to you as
# well as other copyrighted, protected or trademarked materials like
# Logos, graphics, fonts, specific documentations and configurations,
# cryptographic keys etc. are subject to a license agreement between
# you and Univention and not subject to the GNU AGPL V3.
#
# In the case you use this program under the terms of the GNU AGPL V3,
# the program is provided in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public
# License with the Debian GNU/Linux or Univention distribution in file
# /usr/share/common-licenses/AGPL-3; if not, see
# <http://www.gnu.org/licenses/>.

from univention.lib.i18n import Translation

from univention.management.console.protocol.definitions import MODULE_ERR_COMMAND_FAILED
from univention.management.console.log import MODULE

from notifier import Callback

_ = Translation('univention-management-console-modules-uvmm').translate


class Cloud(object):
	"""
	Handle cloud connections and instances.
	"""

	#FIXME @sanitize(nodePattern=SearchSanitizer(default='*'))
	def cloud_query(self, request):
		"""
		Searches clouds by the given pattern

		options: {'nodePattern': <cloud pattern>}

		return: [{
			'id': <cloud name>,
			'label': <cloud name>,
			'group': 'cloudconnection',
			'type': 'cloud',
			'available': (True|False),
			}, ...]
		"""
		#FIXME self.required_options(request, 'nodePattern')

		def _finished(thread, result, request):
			"""
			Process asynchronous UVMM CLOUD_LIST answer.
			"""
			if self._check_thread_error(thread, result, request):
				return

			clouds = []
			success, data = result

			if success:
				for d in data:
					clouds.append({
						'id': d.name,
						'label': d.name,
						'group': _('Cloud connection'),
						'type': 'cloud',
						'available': False,  # FIXME d.last_try == d.last_update,
						})

				MODULE.info('success: %s, data: %s' % (success, clouds))
				self.finished(request.id, clouds)
			else:
				self.finished(
						request.id,
						None,
						message=str(data),
						status=MODULE_ERR_COMMAND_FAILED
						)

		self.uvmm.send(
				'L_CLOUD_LIST',
				Callback(_finished, request),
				#FIXME pattern=request.options['nodePattern']
				)
